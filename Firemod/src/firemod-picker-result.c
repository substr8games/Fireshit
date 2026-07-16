#include "firemod-color-picker-private.h"

#include <gtk4-layer-shell.h>

static gboolean result_close_requested(GtkWindow *window, gpointer user_data)
{
    FiremodColorPicker *picker = user_data;
    if (picker->result_window == window) picker->result_window = NULL;
    return FALSE;
}

static void destroy_result(FiremodColorPicker *picker)
{
    if (picker->result_window == NULL) return;
    gtk_window_destroy(picker->result_window);
    picker->result_window = NULL;
}

static void copy_value(GtkButton *button, gpointer user_data)
{
    (void)user_data;
    const char *value = g_object_get_data(G_OBJECT(button), "firemod-color");
    if (value == NULL) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(button));
    gdk_clipboard_set_text(clipboard, value);
}

static GtkWidget *value_button(const char *value)
{
    GtkWidget *button = gtk_button_new_with_label(value);
    gtk_widget_add_css_class(button, "picker-value");
    g_object_set_data_full(G_OBJECT(button), "firemod-color",
        g_strdup(value), g_free);
    g_signal_connect(button, "clicked", G_CALLBACK(copy_value), NULL);
    return button;
}

static void draw_swatch(
    GtkDrawingArea *area,
    cairo_t *cr,
    int width,
    int height,
    gpointer user_data)
{
    (void)area;
    FiremodColor *color = user_data;
    cairo_set_source_rgb(cr, color->red / 255.0,
        color->green / 255.0, color->blue / 255.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

static gboolean result_key(
    GtkEventControllerKey *controller,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    (void)controller;
    (void)keycode;
    (void)state;
    if (keyval != GDK_KEY_Escape) return FALSE;
    destroy_result(user_data);
    return TRUE;
}

void firemod_picker_result_show(
    FiremodColorPicker *picker,
    FiremodColor *color)
{
    destroy_result(picker);
    GtkWindow *window = GTK_WINDOW(
        gtk_application_window_new(picker->application));
    picker->result_window = window;
    g_signal_connect(window, "close-request",
        G_CALLBACK(result_close_requested), picker);
    gtk_widget_add_css_class(GTK_WIDGET(window), "picker-result");
    gtk_window_set_decorated(window, FALSE);
    gtk_window_set_default_size(window, 246, -1);
    gtk_layer_init_for_window(window);
    gtk_layer_set_namespace(window, "firemod-color-result");
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_monitor(window, picker->monitor);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_keyboard_mode(window,
        GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gint left = CLAMP((gint)picker->pointer_x + 12, 2,
        MAX(2, picker->geometry.width - 250));
    gint top = CLAMP((gint)picker->pointer_y + 12, 2,
        MAX(2, picker->geometry.height - 156));
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, left);
    gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, top);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(box, "picker-result-panel");
    GtkWidget *title = gtk_label_new("COLOR COPIED");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "picker-result-title");
    GtkWidget *swatch = gtk_drawing_area_new();
    gtk_widget_set_size_request(swatch, -1, 24);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(swatch),
        draw_swatch, color, NULL);
    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), swatch);
    gtk_box_append(GTK_BOX(box), value_button(color->hex));
    gtk_box_append(GTK_BOX(box), value_button(color->rgb));
    gtk_box_append(GTK_BOX(box), value_button(color->hsl));
    gtk_box_append(GTK_BOX(box), value_button(color->oklch));
    gtk_window_set_child(window, box);

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(result_key), picker);
    gtk_widget_add_controller(GTK_WIDGET(window), keys);
    g_object_set_data_full(G_OBJECT(window), "firemod-result-color",
        color, (GDestroyNotify)firemod_color_free);
    gtk_window_present(window);
}
