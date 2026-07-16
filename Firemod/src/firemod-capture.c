#include "firemod-capture-private.h"

#include <glib/gstdio.h>
#include <gtk4-layer-shell.h>
#include <math.h>
#include <string.h>

static GdkMonitor *monitor_at(GdkDisplay *display, double x, double y)
{
    GListModel *monitors = gdk_display_get_monitors(display);
    for (guint i = 0; i < g_list_model_get_n_items(monitors); i++) {
        GdkMonitor *monitor = g_list_model_get_item(monitors, i);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        if (x >= geometry.x && y >= geometry.y &&
            x < geometry.x + geometry.width &&
            y < geometry.y + geometry.height) {
            return monitor;
        }
        g_object_unref(monitor);
    }
    return NULL;
}

void firemod_capture_report_error(const gchar *detail)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Screen capture failed");
    gtk_alert_dialog_set_detail(dialog, detail != NULL ? detail :
        "The requested screen capture could not be completed");
    gtk_alert_dialog_show(dialog, NULL);
    g_object_unref(dialog);
}

static gchar *capture_path(FiremodCaptureMode mode)
{
    const gchar *pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    gchar *base = pictures != NULL ? g_strdup(pictures) :
        g_build_filename(g_get_home_dir(), "Pictures", NULL);
    gchar *directory = g_build_filename(base, "Fireshit", NULL);
    g_free(base);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_free(directory);
        return NULL;
    }
    GDateTime *now = g_date_time_new_now_local();
    gchar *stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
    g_date_time_unref(now);
    const gchar *kind = mode == FIREMOD_CAPTURE_REGION ? "snip" : "screenshot";
    gchar *path = NULL;
    for (guint suffix = 0; suffix < 1000; suffix++) {
        gchar *name = suffix == 0 ?
            g_strdup_printf("fireshit-%s-%s.png", kind, stamp) :
            g_strdup_printf("fireshit-%s-%s-%02u.png", kind, stamp, suffix);
        path = g_build_filename(directory, name, NULL);
        g_free(name);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) break;
        g_clear_pointer(&path, g_free);
    }
    g_free(stamp);
    g_free(directory);
    return path;
}

static void report_saved(FiremodCaptureRequest *request, const gchar *path)
{
    GNotification *notice = g_notification_new(
        request->mode == FIREMOD_CAPTURE_REGION ? "Screen snip saved" :
            "Screenshot saved");
    g_notification_set_body(notice, path);
    gchar *id = g_compute_checksum_for_string(G_CHECKSUM_SHA256, path, -1);
    g_application_send_notification(G_APPLICATION(request->app->application),
        id, notice);
    g_free(id);
    g_object_unref(notice);
}

void firemod_capture_request_free(FiremodCaptureRequest *request)
{
    if (request == NULL) return;
    GtkWindow *window = request->window;
    request->window = NULL;
    if (window != NULL) {
        g_signal_handlers_disconnect_by_data(window, request);
        gtk_window_destroy(window);
    }
    g_clear_object(&request->monitor);
    firemod_capture_frame_free(request->frame);
    g_free(request);
}

gboolean firemod_capture_save_region(
    FiremodCaptureRequest *request,
    double logical_x,
    double logical_y,
    double logical_width,
    double logical_height,
    gchar **saved_path)
{
    FiremodCaptureFrame *frame = request->frame;
    double sx = frame->width / (double)MAX(1, request->geometry.width);
    double sy = frame->height / (double)MAX(1, request->geometry.height);
    gint x = CLAMP((gint)floor(logical_x * sx), 0, (gint)frame->width - 1);
    gint y = CLAMP((gint)floor(logical_y * sy), 0, (gint)frame->height - 1);
    gint x2 = CLAMP((gint)ceil((logical_x + logical_width) * sx),
        x + 1, (gint)frame->width);
    gint y2 = CLAMP((gint)ceil((logical_y + logical_height) * sy),
        y + 1, (gint)frame->height);
    gint width = x2 - x;
    gint height = y2 - y;
    gsize stride = (gsize)width * 4;
    guint8 *pixels = g_malloc(stride * (gsize)height);
    gsize source_size = 0;
    const guint8 *source = g_bytes_get_data(frame->rgba, &source_size);
    for (gint row = 0; row < height; row++) {
        gsize offset = (gsize)(y + row) * frame->stride + (gsize)x * 4;
        if (offset + stride > source_size) {
            g_free(pixels);
            return FALSE;
        }
        memcpy(pixels + row * stride, source + offset, stride);
    }
    GBytes *bytes = g_bytes_new_take(pixels, stride * (gsize)height);
    GdkTexture *texture = GDK_TEXTURE(gdk_memory_texture_new(width, height,
        GDK_MEMORY_R8G8B8A8, bytes, stride));
    g_bytes_unref(bytes);
    gchar *path = capture_path(request->mode);
    gboolean saved = path != NULL && gdk_texture_save_to_png(texture, path);
    g_object_unref(texture);
    if (saved) report_saved(request, path);
    if (saved_path != NULL) *saved_path = saved ? g_strdup(path) : NULL;
    g_free(path);
    return saved;
}

static void capture_ready(
    FiremodCaptureFrame *frame,
    const GError *error,
    gpointer user_data)
{
    FiremodCaptureRequest *request = user_data;
    if (error != NULL || frame == NULL) {
        firemod_capture_report_error(error != NULL ? error->message : NULL);
        firemod_capture_request_free(request);
        return;
    }
    request->frame = frame;
    if (request->mode == FIREMOD_CAPTURE_REGION) {
        firemod_capture_show_selector(request);
        return;
    }
    if (!firemod_capture_save_region(request, 0, 0,
            request->geometry.width, request->geometry.height, NULL)) {
        firemod_capture_report_error(
            "The screenshot could not be written to Pictures/Fireshit");
    }
    firemod_capture_request_free(request);
}

static void begin_capture(
    FiremodApp *app,
    FiremodCaptureMode mode,
    double global_x,
    double global_y)
{
    if (app == NULL || app->screencopy == NULL) {
        firemod_capture_report_error("Native Wayfire capture is unavailable");
        return;
    }
    if (mode == FIREMOD_CAPTURE_REGION && !gtk_layer_is_supported()) {
        firemod_capture_report_error(
            "Screen snipping requires GTK4 Layer Shell on Wayland");
        return;
    }
    FiremodCaptureRequest *request = g_new0(FiremodCaptureRequest, 1);
    request->app = app;
    request->mode = mode;
    request->monitor = monitor_at(gdk_display_get_default(), global_x, global_y);
    if (request->monitor == NULL) {
        firemod_capture_report_error(
            "No output exists beneath the invocation pointer");
        firemod_capture_request_free(request);
        return;
    }
    gdk_monitor_get_geometry(request->monitor, &request->geometry);
    GError *error = NULL;
    if (!firemod_screencopy_capture(app->screencopy, request->monitor,
            capture_ready, request, &error)) {
        firemod_capture_report_error(error != NULL ? error->message : NULL);
        g_clear_error(&error);
        firemod_capture_request_free(request);
    }
}

void firemod_capture_output(FiremodApp *app, double x, double y)
{ begin_capture(app, FIREMOD_CAPTURE_OUTPUT, x, y); }

void firemod_capture_region(FiremodApp *app, double x, double y)
{ begin_capture(app, FIREMOD_CAPTURE_REGION, x, y); }
