#include "firemod-screencopy-private.h"

#include <gdk/wayland/gdkwayland.h>
#include <gio/gio.h>
#include <stdint.h>

static void registry_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version)
{
    FiremodScreencopy *copy = data;
    if (g_strcmp0(interface, wl_shm_interface.name) == 0) {
        copy->shm = wl_registry_bind(registry, name,
            &wl_shm_interface, MIN(version, 1));
    } else if (g_strcmp0(interface,
            zwlr_screencopy_manager_v1_interface.name) == 0) {
        copy->manager = wl_registry_bind(registry, name,
            &zwlr_screencopy_manager_v1_interface, MIN(version, 3));
    }
}

static void registry_remove(
    void *data,
    struct wl_registry *registry,
    uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

FiremodScreencopy *firemod_screencopy_new(
    GdkDisplay *display,
    GError **error)
{
    if (!GDK_IS_WAYLAND_DISPLAY(display)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
            "Color Picker requires the native Wayland display backend");
        return NULL;
    }
    FiremodScreencopy *copy = g_new0(FiremodScreencopy, 1);
    copy->pending = g_ptr_array_new();
    copy->display = gdk_wayland_display_get_wl_display(display);
    copy->registry = wl_display_get_registry(copy->display);
    wl_registry_add_listener(copy->registry, &registry_listener, copy);
    wl_display_roundtrip(copy->display);
    if (copy->shm == NULL || copy->manager == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
            "Wayfire did not advertise wlr-screencopy-v1");
        firemod_screencopy_free(copy);
        return NULL;
    }
    return copy;
}

void firemod_screencopy_free(FiremodScreencopy *copy)
{
    if (copy == NULL) return;
    firemod_screencopy_frame_cancel_all(copy);
    if (copy->manager != NULL) {
        zwlr_screencopy_manager_v1_destroy(copy->manager);
    }
    if (copy->shm != NULL) wl_shm_destroy(copy->shm);
    if (copy->registry != NULL) wl_registry_destroy(copy->registry);
    g_ptr_array_free(copy->pending, TRUE);
    g_free(copy);
}

gboolean firemod_screencopy_capture(
    FiremodScreencopy *copy,
    GdkMonitor *monitor,
    FiremodCaptureCallback callback,
    gpointer user_data,
    GError **error)
{
    if (copy == NULL || monitor == NULL || callback == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Invalid pixel capture request");
        return FALSE;
    }
    struct wl_output *output = gdk_wayland_monitor_get_wl_output(monitor);
    if (output == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
            "The selected monitor has no native Wayland output");
        return FALSE;
    }
    return firemod_screencopy_frame_begin(
        copy, output, callback, user_data, error);
}

void firemod_capture_frame_free(FiremodCaptureFrame *frame)
{
    if (frame == NULL) return;
    g_bytes_unref(frame->rgba);
    g_free(frame);
}
