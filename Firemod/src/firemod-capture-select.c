#include "firemod-capture-private.h"

#include <gtk4-layer-shell.h>
#include <math.h>
#include <pango/pangocairo.h>

static void bounds(FiremodCaptureRequest *request,
    double *x, double *y, double *width, double *height)
{
    *x = MIN(request->start_x, request->end_x);
    *y = MIN(request->start_y, request->end_y);
    *width = fabs(request->end_x - request->start_x);
    *height = fabs(request->end_y - request->start_y);
}

static void draw_selector(GtkDrawingArea *area, cairo_t *cr,
    gint width, gint height, gpointer user_data)
{
    (void)area;
    FiremodCaptureRequest *request = user_data;
    cairo_set_source_rgba(cr, 0, 0, 0, 0.44);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    if (!request->dragging && request->start_x == request->end_x &&
        request->start_y == request->end_y) return;
    double x, y, selected_width, selected_height;
    bounds(request, &x, &y, &selected_width, &selected_height);
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_rectangle(cr, x, y, selected_width, selected_height);
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_set_source_rgba(cr, 0.33, 0.79, 0.91, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, x + 1, y + 1,
        MAX(0, selected_width - 2), MAX(0, selected_height - 2));
    cairo_stroke(cr);
    gchar *size = g_strdup_printf("%d × %d",
        (gint)selected_width, (gint)selected_height);
    PangoLayout *layout = gtk_widget_create_pango_layout(
        GTK_WIDGET(request->window), size);
    cairo_set_source_rgba(cr, 0.91, 0.94, 0.96, 1.0);
    cairo_move_to(cr, CLAMP(x, 8.0, MAX(8.0, width - 100.0)),
        MAX(8.0, y - 22.0));
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    g_free(size);
}

static void motion(GtkEventControllerMotion *controller,
    double x, double y, gpointer user_data)
{
    FiremodCaptureRequest *request = user_data;
    if (!request->dragging) return;
    request->end_x = CLAMP(x, 0, request->geometry.width);
    request->end_y = CLAMP(y, 0, request->geometry.height);
    gtk_widget_queue_draw(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(controller)));
}

static void pressed(GtkGestureClick *gesture, gint presses,
    double x, double y, gpointer user_data)
{
    (void)presses;
    FiremodCaptureRequest *request = user_data;
    guint button = gtk_gesture_single_get_current_button(
        GTK_GESTURE_SINGLE(gesture));
    if (button == GDK_BUTTON_SECONDARY) {
        firemod_capture_request_free(request);
        return;
    }
    if (button != GDK_BUTTON_PRIMARY) return;
    request->dragging = TRUE;
    request->start_x = request->end_x =
        CLAMP(x, 0, request->geometry.width);
    request->start_y = request->end_y =
        CLAMP(y, 0, request->geometry.height);
    gtk_widget_queue_draw(gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)));
}

static void released(GtkGestureClick *gesture, gint presses,
    double x, double y, gpointer user_data)
{
    (void)presses;
    FiremodCaptureRequest *request = user_data;
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) !=
            GDK_BUTTON_PRIMARY || !request->dragging) return;
    request->dragging = FALSE;
    request->end_x = CLAMP(x, 0, request->geometry.width);
    request->end_y = CLAMP(y, 0, request->geometry.height);
    double selected_x, selected_y, selected_width, selected_height;
    bounds(request, &selected_x, &selected_y,
        &selected_width, &selected_height);
    if (selected_width < 4 || selected_height < 4) {
        request->start_x = request->end_x = 0;
        request->start_y = request->end_y = 0;
        gtk_widget_queue_draw(GTK_WIDGET(request->window));
        return;
    }
    if (!firemod_capture_save_region(request, selected_x, selected_y,
            selected_width, selected_height, NULL)) {
        firemod_capture_report_error(
            "The screen snip could not be written to Pictures/Fireshit");
    }
    firemod_capture_request_free(request);
}

static gboolean key_pressed(GtkEventControllerKey *controller,
    guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    (void)controller;
    (void)keycode;
    (void)state;
    if (keyval != GDK_KEY_Escape) return FALSE;
    firemod_capture_request_free(user_data);
    return TRUE;
}

static gboolean close_requested(GtkWindow *window, gpointer user_data)
{
    FiremodCaptureRequest *request = user_data;
    if (request->window == window) request->window = NULL;
    firemod_capture_request_free(request);
    return FALSE;
}

void firemod_capture_show_selector(FiremodCaptureRequest *request)
{
    GtkWindow *window = GTK_WINDOW(
        gtk_application_window_new(request->app->application));
    request->window = window;
    g_signal_connect(window, "close-request",
        G_CALLBACK(close_requested), request);
    gtk_widget_add_css_class(GTK_WIDGET(window), "screenshot-overlay");
    gtk_window_set_decorated(window, FALSE);
    gtk_layer_init_for_window(window);
    gtk_layer_set_namespace(window, "firemod-screen-snip");
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_monitor(window, request->monitor);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_keyboard_mode(window,
        GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    GdkTexture *texture = GDK_TEXTURE(gdk_memory_texture_new(
        request->frame->width, request->frame->height,
        GDK_MEMORY_R8G8B8A8, request->frame->rgba,
        request->frame->stride));
    GtkWidget *picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_FILL);
    g_object_unref(texture);
    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), picture);
    GtkWidget *drawing = gtk_drawing_area_new();
    gtk_widget_set_hexpand(drawing, TRUE);
    gtk_widget_set_vexpand(drawing, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing),
        draw_selector, request, NULL);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing);
    gtk_window_set_child(window, overlay);
    gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "crosshair");
    GtkEventController *move = gtk_event_controller_motion_new();
    g_signal_connect(move, "motion", G_CALLBACK(motion), request);
    gtk_widget_add_controller(drawing, move);
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(click, "pressed", G_CALLBACK(pressed), request);
    g_signal_connect(click, "released", G_CALLBACK(released), request);
    gtk_widget_add_controller(drawing, GTK_EVENT_CONTROLLER(click));
    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(key_pressed), request);
    gtk_widget_add_controller(GTK_WIDGET(window), keys);
    gtk_window_present(window);
}
