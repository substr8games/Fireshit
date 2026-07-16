#include "firemod-color-picker-private.h"

#include <gtk4-layer-shell.h>

static gboolean picker_window_close_requested(
    GtkWindow *window,
    gpointer user_data)
{
    FiremodColorPicker *picker = user_data;
    if (picker->window == window) picker->window = NULL;
    return FALSE;
}

static void destroy_window(GtkWindow **window)
{
    if (*window == NULL) return;
    gtk_window_destroy(*window);
    *window = NULL;
}

static void sample_physical(
    FiremodColorPicker *picker,
    gint px,
    gint py,
    guint8 *red,
    guint8 *green,
    guint8 *blue)
{
    px = CLAMP(px, 0, (gint)picker->frame->width - 1);
    py = CLAMP(py, 0, (gint)picker->frame->height - 1);
    gsize size = 0;
    const guint8 *pixels = g_bytes_get_data(picker->frame->rgba, &size);
    gsize offset = py * picker->frame->stride + px * 4;
    *red = pixels[offset];
    *green = pixels[offset + 1];
    *blue = pixels[offset + 2];
}

static void draw_picker(
    GtkDrawingArea *area,
    cairo_t *cr,
    int width,
    int height,
    gpointer user_data)
{
    (void)area;
    FiremodColorPicker *picker = user_data;
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
    cairo_move_to(cr, picker->pointer_x - 10, picker->pointer_y + 0.5);
    cairo_line_to(cr, picker->pointer_x + 10, picker->pointer_y + 0.5);
    cairo_move_to(cr, picker->pointer_x + 0.5, picker->pointer_y - 10);
    cairo_line_to(cr, picker->pointer_x + 0.5, picker->pointer_y + 10);
    cairo_stroke(cr);

    const gint cells = 9;
    const gint cell = 8;
    gint mx = CLAMP((gint)picker->pointer_x + 18, 2,
        MAX(2, width - cells * cell - 4));
    gint my = CLAMP((gint)picker->pointer_y + 18, 2,
        MAX(2, height - cells * cell - 4));
    double sx = picker->frame->width / (double)MAX(1, picker->geometry.width);
    double sy = picker->frame->height / (double)MAX(1, picker->geometry.height);
    gint center_x = picker->pointer_x * sx;
    gint center_y = picker->pointer_y * sy;
    for (gint y = 0; y < cells; y++) {
        for (gint x = 0; x < cells; x++) {
            guint8 r, g, b;
            sample_physical(picker, center_x + x - cells / 2,
                center_y + y - cells / 2, &r, &g, &b);
            cairo_set_source_rgb(cr, r / 255.0, g / 255.0, b / 255.0);
            cairo_rectangle(cr, mx + x * cell, my + y * cell, cell, cell);
            cairo_fill(cr);
        }
    }
    cairo_set_source_rgb(cr, 1.0, 0.69, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, mx + (cells / 2) * cell,
        my + (cells / 2) * cell, cell, cell);
    cairo_stroke(cr);
}

static void pointer_motion(
    GtkEventControllerMotion *motion,
    double x,
    double y,
    gpointer user_data)
{
    FiremodColorPicker *picker = user_data;
    picker->pointer_x = x;
    picker->pointer_y = y;
    gtk_widget_queue_draw(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(motion)));
}

static void picker_click(
    GtkGestureClick *gesture,
    gint presses,
    double x,
    double y,
    gpointer user_data)
{
    (void)presses;
    FiremodColorPicker *picker = user_data;
    picker->pointer_x = x;
    picker->pointer_y = y;
    guint button = gtk_gesture_single_get_current_button(
        GTK_GESTURE_SINGLE(gesture));
    if (button == GDK_BUTTON_PRIMARY) firemod_picker_accept(picker);
    else if (button == GDK_BUTTON_SECONDARY) firemod_picker_cancel(picker);
}

static gboolean picker_key(
    GtkEventControllerKey *controller,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    (void)controller; (void)keycode; (void)state;
    if (keyval != GDK_KEY_Escape) return FALSE;
    firemod_picker_cancel(user_data);
    return TRUE;
}

void firemod_picker_view_show(FiremodColorPicker *picker)
{
    destroy_window(&picker->window);
    GtkWindow *window = GTK_WINDOW(
        gtk_application_window_new(picker->application));
    picker->window = window;
    g_signal_connect(window, "close-request",
        G_CALLBACK(picker_window_close_requested), picker);
    gtk_widget_add_css_class(GTK_WIDGET(window), "picker-overlay");
    gtk_window_set_decorated(window, FALSE);
    gtk_layer_init_for_window(window);
    gtk_layer_set_namespace(window, "firemod-color-picker");
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_monitor(window, picker->monitor);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_keyboard_mode(window,
        GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    GdkTexture *texture = GDK_TEXTURE(gdk_memory_texture_new(
        picker->frame->width, picker->frame->height,
        GDK_MEMORY_R8G8B8A8, g_bytes_ref(picker->frame->rgba),
        picker->frame->stride));
    GtkWidget *picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_FILL);
    g_object_unref(texture);
    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), picture);
    GtkWidget *drawing = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawing, TRUE);
    gtk_widget_set_vexpand(drawing, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing),
        draw_picker, picker, NULL);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing);
    gtk_window_set_child(window, overlay);
    gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "crosshair");

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(pointer_motion), picker);
    gtk_widget_add_controller(drawing, motion);
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(picker_click), picker);
    gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(click));
    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(picker_key), picker);
    gtk_widget_add_controller(GTK_WIDGET(window), keys);
    gtk_window_present(window);
}

void firemod_picker_accept(FiremodColorPicker *picker)
{
    guint8 red, green, blue;
    firemod_picker_sample(picker, picker->pointer_x, picker->pointer_y,
        &red, &green, &blue);
    FiremodColor *color = firemod_color_new(red, green, blue);
    GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(picker->window));
    gdk_clipboard_set_text(clipboard, color->hex);
    destroy_window(&picker->window);
    firemod_picker_result_show(picker, color);
}

void firemod_picker_view_close(FiremodColorPicker *picker)
{
    if (picker == NULL) return;
    destroy_window(&picker->window);
    destroy_window(&picker->result_window);
}
