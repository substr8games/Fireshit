#include "firemod-color-picker-private.h"

#include <gtk4-layer-shell.h>

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
    return g_list_model_get_n_items(monitors) > 0 ?
        g_list_model_get_item(monitors, 0) : NULL;
}

static void show_error(const char *message)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Color Picker unavailable");
    gtk_alert_dialog_set_detail(dialog, message);
    gtk_alert_dialog_show(dialog, NULL);
    g_object_unref(dialog);
}

static void capture_ready(
    FiremodCaptureFrame *frame,
    const GError *error,
    gpointer user_data)
{
    FiremodColorPicker *picker = user_data;
    picker->capture_pending = FALSE;
    if (error != NULL || frame == NULL) {
        show_error(error != NULL ? error->message : "Pixel capture failed");
        return;
    }
    firemod_capture_frame_free(picker->frame);
    picker->frame = frame;
    firemod_picker_view_show(picker);
}

FiremodColorPicker *firemod_color_picker_new(
    GtkApplication *application,
    FiremodScreencopy *screencopy)
{
    FiremodColorPicker *picker = g_new0(FiremodColorPicker, 1);
    picker->application = application;
    picker->screencopy = screencopy;
    return picker;
}

void firemod_color_picker_start(
    FiremodColorPicker *picker,
    double global_x,
    double global_y)
{
    g_return_if_fail(picker != NULL);
    firemod_picker_view_close(picker);
    if (picker->capture_pending) {
        show_error("A pixel capture is already in progress");
        return;
    }
    if (picker->screencopy == NULL || !gtk_layer_is_supported()) {
        show_error("Native Wayfire pixel sampling is not available");
        return;
    }
    g_clear_object(&picker->monitor);
    GdkDisplay *display = gdk_display_get_default();
    picker->monitor = monitor_at(display, global_x, global_y);
    if (picker->monitor == NULL) {
        show_error("No output exists beneath the invocation pointer");
        return;
    }
    gdk_monitor_get_geometry(picker->monitor, &picker->geometry);
    picker->pointer_x = global_x - picker->geometry.x;
    picker->pointer_y = global_y - picker->geometry.y;
    GError *error = NULL;
    picker->capture_pending = TRUE;
    if (!firemod_screencopy_capture(picker->screencopy,
            picker->monitor, capture_ready, picker, &error)) {
        picker->capture_pending = FALSE;
        show_error(error != NULL ? error->message : "Pixel capture failed");
        g_clear_error(&error);
    }
}

void firemod_picker_sample(
    FiremodColorPicker *picker,
    double x,
    double y,
    guint8 *red,
    guint8 *green,
    guint8 *blue)
{
    if (picker == NULL || picker->frame == NULL) {
        *red = *green = *blue = 0;
        return;
    }
    double sx = picker->geometry.width > 0 ?
        picker->frame->width / (double)picker->geometry.width : 1.0;
    double sy = picker->geometry.height > 0 ?
        picker->frame->height / (double)picker->geometry.height : 1.0;
    guint px = CLAMP((gint)(x * sx), 0, (gint)picker->frame->width - 1);
    guint py = CLAMP((gint)(y * sy), 0, (gint)picker->frame->height - 1);
    gsize size = 0;
    const guint8 *pixels = g_bytes_get_data(picker->frame->rgba, &size);
    gsize offset = py * picker->frame->stride + px * 4;
    if (offset + 2 >= size) {
        *red = *green = *blue = 0;
        return;
    }
    *red = pixels[offset];
    *green = pixels[offset + 1];
    *blue = pixels[offset + 2];
}

void firemod_picker_cancel(FiremodColorPicker *picker)
{
    firemod_picker_view_close(picker);
}

void firemod_color_picker_free(FiremodColorPicker *picker)
{
    if (picker == NULL) return;
    firemod_picker_view_close(picker);
    g_clear_object(&picker->monitor);
    firemod_capture_frame_free(picker->frame);
    g_free(picker);
}
