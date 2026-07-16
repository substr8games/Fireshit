#include "firemod-screencopy-private.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct {
    FiremodScreencopy *owner;
    struct zwlr_screencopy_frame_v1 *protocol;
    struct wl_buffer *buffer;
    void *map;
    gsize map_size;
    guint32 format;
    guint width;
    guint height;
    guint stride;
    gboolean y_invert;
    FiremodCaptureCallback callback;
    gpointer user_data;
} PendingCapture;

static void pending_free(PendingCapture *pending)
{
    if (pending == NULL) return;
    if (pending->owner != NULL && pending->owner->pending != NULL) {
        g_ptr_array_remove_fast(pending->owner->pending, pending);
    }
    if (pending->protocol != NULL) {
        zwlr_screencopy_frame_v1_destroy(pending->protocol);
    }
    if (pending->buffer != NULL) wl_buffer_destroy(pending->buffer);
    if (pending->map != NULL) munmap(pending->map, pending->map_size);
    g_free(pending);
}

static void report_failure(PendingCapture *pending, const char *message)
{
    GError *error = g_error_new_literal(
        G_IO_ERROR, G_IO_ERROR_FAILED, message);
    pending->callback(NULL, error, pending->user_data);
    g_error_free(error);
    pending_free(pending);
}

static int create_shm(gsize size, void **map, GError **error)
{
    char *name = NULL;
    int fd = g_file_open_tmp("firemod-pixels-XXXXXX", &name, error);
    if (fd < 0) return -1;
    unlink(name);
    g_free(name);
    if (ftruncate(fd, (off_t)size) != 0) {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
            "Could not size screencopy buffer: %s", g_strerror(errno));
        close(fd);
        return -1;
    }
    *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*map == MAP_FAILED) {
        *map = NULL;
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
            "Could not map screencopy buffer: %s", g_strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static gboolean submit_copy(PendingCapture *pending)
{
    if (pending->buffer != NULL) return TRUE;
    if (pending->format != WL_SHM_FORMAT_XRGB8888 &&
        pending->format != WL_SHM_FORMAT_ARGB8888) {
        report_failure(pending, "Wayfire offered an unsupported pixel format");
        return FALSE;
    }
    GError *error = NULL;
    if (pending->stride == 0 || pending->height == 0 ||
        pending->stride > G_MAXSIZE / pending->height) {
        report_failure(pending, "Wayfire offered invalid pixel dimensions");
        return FALSE;
    }
    pending->map_size = (gsize)pending->stride * pending->height;
    int fd = create_shm(pending->map_size, &pending->map, &error);
    if (fd < 0) {
        char *message = g_strdup(error != NULL ? error->message :
            "Could not allocate screencopy buffer");
        g_clear_error(&error);
        report_failure(pending, message);
        g_free(message);
        return FALSE;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(
        pending->owner->shm, fd, pending->map_size);
    pending->buffer = wl_shm_pool_create_buffer(pool, 0,
        pending->width, pending->height, pending->stride, pending->format);
    wl_shm_pool_destroy(pool);
    close(fd);
    zwlr_screencopy_frame_v1_copy(pending->protocol, pending->buffer);
    return TRUE;
}

static FiremodCaptureFrame *normalize(PendingCapture *pending)
{
    gsize stride = (gsize)pending->width * 4;
    guint8 *rgba = g_malloc(stride * pending->height);
    const guint8 *source = pending->map;
    for (guint y = 0; y < pending->height; y++) {
        guint sy = pending->y_invert ? pending->height - 1 - y : y;
        const guint32 *row = (const guint32*)(source + sy * pending->stride);
        guint8 *out = rgba + y * stride;
        for (guint x = 0; x < pending->width; x++) {
            guint32 pixel = row[x];
            out[x * 4] = (pixel >> 16) & 0xff;
            out[x * 4 + 1] = (pixel >> 8) & 0xff;
            out[x * 4 + 2] = pixel & 0xff;
            out[x * 4 + 3] = 0xff;
        }
    }
    FiremodCaptureFrame *frame = g_new0(FiremodCaptureFrame, 1);
    frame->width = pending->width;
    frame->height = pending->height;
    frame->stride = stride;
    frame->rgba = g_bytes_new_take(rgba, stride * pending->height);
    return frame;
}

static void on_buffer(void *data, struct zwlr_screencopy_frame_v1 *frame,
    uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    PendingCapture *pending = data;
    pending->format = format;
    pending->width = width;
    pending->height = height;
    pending->stride = stride;
    if (width == 0 || height == 0 || stride < width * 4) {
        report_failure(pending, "Wayfire offered invalid pixel dimensions");
        return;
    }
    if (wl_proxy_get_version((struct wl_proxy*)frame) < 3) submit_copy(pending);
}

static void on_flags(void *data, struct zwlr_screencopy_frame_v1 *frame,
    uint32_t flags)
{
    (void)frame;
    PendingCapture *pending = data;
    pending->y_invert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

static void on_ready(void *data, struct zwlr_screencopy_frame_v1 *frame,
    uint32_t sec_hi, uint32_t sec_lo, uint32_t nsec)
{
    (void)frame; (void)sec_hi; (void)sec_lo; (void)nsec;
    PendingCapture *pending = data;
    pending->callback(normalize(pending), NULL, pending->user_data);
    pending_free(pending);
}

static void on_failed(void *data, struct zwlr_screencopy_frame_v1 *frame)
{
    (void)frame;
    report_failure(data, "Wayfire rejected the pixel capture request");
}

static void on_damage(void *data, struct zwlr_screencopy_frame_v1 *frame,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{ (void)data; (void)frame; (void)x; (void)y; (void)width; (void)height; }

static void on_linux_dmabuf(void *data,
    struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
    uint32_t width, uint32_t height)
{ (void)data; (void)frame; (void)format; (void)width; (void)height; }

static void on_buffer_done(void *data,
    struct zwlr_screencopy_frame_v1 *frame)
{ (void)frame; submit_copy(data); }

static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .buffer = on_buffer,
    .flags = on_flags,
    .ready = on_ready,
    .failed = on_failed,
    .damage = on_damage,
    .linux_dmabuf = on_linux_dmabuf,
    .buffer_done = on_buffer_done,
};

void firemod_screencopy_frame_cancel_all(FiremodScreencopy *copy)
{
    if (copy == NULL || copy->pending == NULL) return;
    while (copy->pending->len > 0) {
        guint index = copy->pending->len - 1;
        PendingCapture *pending = g_ptr_array_index(copy->pending, index);
        g_ptr_array_remove_index_fast(copy->pending, index);
        pending->owner = NULL;
        pending_free(pending);
    }
}

gboolean firemod_screencopy_frame_begin(
    FiremodScreencopy *copy,
    struct wl_output *output,
    FiremodCaptureCallback callback,
    gpointer user_data,
    GError **error)
{
    PendingCapture *pending = g_new0(PendingCapture, 1);
    pending->owner = copy;
    pending->callback = callback;
    pending->user_data = user_data;
    g_ptr_array_add(copy->pending, pending);
    pending->protocol = zwlr_screencopy_manager_v1_capture_output(
        copy->manager, 0, output);
    if (pending->protocol == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "Wayfire refused to create a screencopy frame");
        pending_free(pending);
        return FALSE;
    }
    zwlr_screencopy_frame_v1_add_listener(
        pending->protocol, &frame_listener, pending);
    wl_display_flush(copy->display);
    return TRUE;
}
