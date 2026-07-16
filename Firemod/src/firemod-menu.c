#include "firemod-menu.h"

#include <gtk4-layer-shell.h>
#include <pango/pangocairo.h>

#include "firemod-hex-assets.h"
#include "firemod-hex-layout.h"

#define CELL_WIDTH FIREMOD_HEX_CELL_WIDTH
#define CELL_HEIGHT FIREMOD_HEX_CELL_HEIGHT
#define ICON_MAX FIREMOD_HEX_ICON_MAX
#define SAFE_MARGIN 4.0

typedef struct {
    FiremodCapability *capability;
    gdouble x;
    gdouble y;
    guint order;
} MenuCell;

struct FiremodMenu {
    GtkApplication *application;
    FiremodCapabilityRegistry *registry;
    FiremodSettings *settings;
    FiremodActionRequest action_request;
    gpointer user_data;
    GtkWindow *window;
    GtkDrawingArea *canvas;
    GPtrArray *cells;
    FiremodHexAssets *assets;
    GdkRectangle monitor_geometry;
    gdouble global_x;
    gdouble global_y;
    gdouble scale;
    gint hovered;
    gint focused;
    gint pending_hover;
    guint hover_timer;
    guint arm_timer;
    gboolean hover_visible;
    gboolean armed;
};

static void cell_free(MenuCell *cell) { g_free(cell); }

static gint capability_order(gconstpointer left, gconstpointer right)
{
    const FiremodCapability *a = *(FiremodCapability * const *)left;
    const FiremodCapability *b = *(FiremodCapability * const *)right;
    if (a->menu_order != b->menu_order) return a->menu_order - b->menu_order;
    return g_strcmp0(a->id, b->id);
}

static GdkMonitor *monitor_at(GdkDisplay *display, double x, double y)
{
    GListModel *monitors = gdk_display_get_monitors(display);
    for (guint i = 0; i < g_list_model_get_n_items(monitors); i++) {
        GdkMonitor *monitor = g_list_model_get_item(monitors, i);
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        if (x >= geometry.x && y >= geometry.y &&
            x < geometry.x + geometry.width && y < geometry.y + geometry.height) {
            return monitor;
        }
        g_object_unref(monitor);
    }
    return g_list_model_get_n_items(monitors) > 0 ?
        g_list_model_get_item(monitors, 0) : NULL;
}

static gboolean parse_color(const gchar *text, GdkRGBA *color)
{
    return text != NULL && gdk_rgba_parse(color, text);
}

static void rgba(cairo_t *cr, const GdkRGBA *color, gdouble alpha)
{
    cairo_set_source_rgba(cr, color->red, color->green, color->blue, alpha);
}

static gboolean is_picker(const FiremodCapability *capability)
{
    return g_strcmp0(capability->id, "utility.color.pick") == 0;
}

static gboolean is_screenshot(const FiremodCapability *capability)
{
    return g_str_has_prefix(capability->id, "capture.screenshot.");
}

static gboolean is_recording(const FiremodCapability *capability)
{
    return g_str_has_prefix(capability->id, "capture.record.");
}

static void draw_picker_gradient(
    cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height)
{
    cairo_pattern_t *gradient = cairo_pattern_create_linear(
        x - width * 0.5, y - height * 0.5,
        x + width * 0.5, y + height * 0.5);
    cairo_pattern_add_color_stop_rgb(gradient, 0.00, 1.0, 0.0, 0.5);
    cairo_pattern_add_color_stop_rgb(gradient, 0.20, 1.0, 0.3, 0.0);
    cairo_pattern_add_color_stop_rgb(gradient, 0.40, 1.0, 0.9, 0.0);
    cairo_pattern_add_color_stop_rgb(gradient, 0.60, 0.0, 0.8, 0.2);
    cairo_pattern_add_color_stop_rgb(gradient, 0.80, 0.0, 0.8, 1.0);
    cairo_pattern_add_color_stop_rgb(gradient, 1.00, 0.6, 0.0, 1.0);
    cairo_set_source(cr, gradient);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(gradient);
}

static void draw_text(
    cairo_t *cr,
    const gchar *text,
    gdouble x,
    gdouble y,
    gint width,
    gboolean bold,
    const GdkRGBA *color)
{
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    pango_layout_set_text(layout, text != NULL ? text : "ACTION", -1);
    PangoFontDescription *font = pango_font_description_from_string(
        bold ? "Sans Bold 9" : "Sans 9");
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);
    gint logical_width = 0, logical_height = 0;
    pango_layout_get_pixel_size(layout, &logical_width, &logical_height);
    rgba(cr, color, 1.0);
    cairo_move_to(cr, x - logical_width * 0.5, y - logical_height * 0.5);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

static void draw_cell(FiremodMenu *menu, cairo_t *cr, MenuCell *cell, guint index)
{
    FiremodCapability *capability = cell->capability;
    gboolean available = capability->state == FIREMOD_ACTION_AVAILABLE;
    gboolean hovered = (gint)index == menu->hovered;
    gboolean focused = (gint)index == menu->focused;
    gdouble width = CELL_WIDTH * menu->scale;
    gdouble height = CELL_HEIGHT * menu->scale;
    GdkRGBA accent = {0.0, 0.27, 0.67, 1.0};
    parse_color(capability->color, &accent);
    firemod_hex_path(cr, cell->x, cell->y, width, height, 1.0);
    if (is_picker(capability)) {
        draw_picker_gradient(cr, cell->x, cell->y, width, height);
    } else {
        gdouble alpha = available ? 0.42 : 0.16;
        if (is_screenshot(capability)) alpha += 0.08;
        if (is_recording(capability)) alpha += 0.05;
        rgba(cr, &accent, alpha);
        cairo_fill_preserve(cr);
    }
    if (hovered || focused) {
        GdkRGBA interaction;
        gdk_rgba_parse(&interaction, "#70d8ff");
        rgba(cr, &interaction, focused ? 0.26 : 0.18);
        cairo_fill_preserve(cr);
    }
    GdkRGBA border;
    gdk_rgba_parse(&border, available ? "#26333d" : "#ff7474");
    if (hovered || focused) gdk_rgba_parse(&border, "#70d8ff");
    rgba(cr, &border, available ? 1.0 : 0.72);
    cairo_set_line_width(cr, focused ? 3.0 : hovered ? 2.0 : 1.0);
    cairo_stroke(cr);
    gdouble opacity = available ? 1.0 : 0.38;
    if (!firemod_hex_assets_draw(menu->assets, cr, capability,
            cell->x, cell->y, ICON_MAX * menu->scale, opacity)) {
        GdkRGBA text = {0.95, 0.95, 0.95, opacity};
        draw_text(cr, capability->short_label, cell->x, cell->y,
            (gint)(width * 0.72), TRUE, &text);
    }
}

static void draw_hover(FiremodMenu *menu, cairo_t *cr, gint width, gint height)
{
    if (!menu->hover_visible || menu->hovered < 0 ||
        (guint)menu->hovered >= menu->cells->len) return;
    MenuCell *cell = g_ptr_array_index(menu->cells, (guint)menu->hovered);
    FiremodCapability *capability = cell->capability;
    const gchar *summary = capability->state == FIREMOD_ACTION_AVAILABLE ?
        capability->summary : capability->availability_reason;
    gdouble plaque_width = 260.0;
    gdouble plaque_height = summary != NULL && *summary != '\0' ? 42.0 : 24.0;
    gdouble x = CLAMP(cell->x - plaque_width * 0.5,
        SAFE_MARGIN, width - plaque_width - SAFE_MARGIN);
    gdouble y = cell->y - CELL_HEIGHT * menu->scale * 0.5 - plaque_height - 4;
    if (y < SAFE_MARGIN) y = cell->y + CELL_HEIGHT * menu->scale * 0.5 + 4;
    y = CLAMP(y, SAFE_MARGIN, height - plaque_height - SAFE_MARGIN);
    GdkRGBA bg, border, title, muted;
    gdk_rgba_parse(&bg, "#0f161d");
    gdk_rgba_parse(&border, "#3a4a57");
    gdk_rgba_parse(&title, "#e5eaee");
    gdk_rgba_parse(&muted, "#94a3ad");
    cairo_rectangle(cr, x, y, plaque_width, plaque_height);
    rgba(cr, &bg, 0.98); cairo_fill_preserve(cr);
    rgba(cr, &border, 1.0); cairo_set_line_width(cr, 1.0); cairo_stroke(cr);
    draw_text(cr, capability->label, x + plaque_width * 0.5,
        y + 12, 250, TRUE, &title);
    if (summary != NULL && *summary != '\0') {
        draw_text(cr, summary, x + plaque_width * 0.5,
            y + 31, 250, FALSE, &muted);
    }
}

static void draw_menu(
    GtkDrawingArea *area,
    cairo_t *cr,
    gint width,
    gint height,
    gpointer user_data)
{
    (void)area;
    FiremodMenu *menu = user_data;
    for (guint i = 0; i < menu->cells->len; i++) {
        draw_cell(menu, cr, g_ptr_array_index(menu->cells, i), i);
    }
    draw_hover(menu, cr, width, height);
}

static gint hit_test(FiremodMenu *menu, gdouble x, gdouble y)
{
    gdouble width = CELL_WIDTH * menu->scale;
    gdouble height = CELL_HEIGHT * menu->scale;
    for (guint i = 0; i < menu->cells->len; i++) {
        MenuCell *cell = g_ptr_array_index(menu->cells, i);
        if (firemod_hex_contains(x, y, cell->x, cell->y, width, height)) {
            return (gint)i;
        }
    }
    return -1;
}

static void update_accessible_action(FiremodMenu *menu, gint index)
{
    if (menu->canvas == NULL || index < 0 ||
        (guint)index >= menu->cells->len) return;
    MenuCell *cell = g_ptr_array_index(menu->cells, (guint)index);
    FiremodCapability *capability = cell->capability;
    const gchar *detail = capability->state == FIREMOD_ACTION_AVAILABLE ?
        capability->summary : capability->availability_reason;
    gtk_accessible_update_property(GTK_ACCESSIBLE(menu->canvas),
        GTK_ACCESSIBLE_PROPERTY_LABEL, capability->label,
        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
        detail != NULL ? detail : "Fireshit utility action",
        -1);
}

static gboolean reveal_hover(gpointer user_data)
{
    FiremodMenu *menu = user_data;
    menu->hover_timer = 0;
    if (menu->pending_hover == menu->hovered && menu->hovered >= 0) {
        menu->hover_visible = TRUE;
        gtk_widget_queue_draw(GTK_WIDGET(menu->canvas));
    }
    return G_SOURCE_REMOVE;
}

static void motion(
    GtkEventControllerMotion *controller,
    gdouble x,
    gdouble y,
    gpointer user_data)
{
    (void)controller;
    FiremodMenu *menu = user_data;
    gint next = hit_test(menu, x, y);
    if (next == menu->hovered) return;
    menu->hovered = next;
    menu->pending_hover = next;
    if (next >= 0) update_accessible_action(menu, next);
    menu->hover_visible = FALSE;
    if (menu->hover_timer != 0) g_source_remove(menu->hover_timer);
    menu->hover_timer = 0;
    if (next >= 0 && menu->settings->active->hover_enabled) {
        menu->hover_timer = g_timeout_add(
            menu->settings->active->hover_delay_ms, reveal_hover, menu);
    }
    gtk_widget_queue_draw(GTK_WIDGET(menu->canvas));
}

static void leave(GtkEventControllerMotion *controller, gpointer user_data)
{
    (void)controller;
    FiremodMenu *menu = user_data;
    menu->hovered = -1;
    menu->hover_visible = FALSE;
    if (menu->hover_timer != 0) g_source_remove(menu->hover_timer);
    menu->hover_timer = 0;
    gtk_widget_queue_draw(GTK_WIDGET(menu->canvas));
}

static void dispatch_index(FiremodMenu *menu, gint index)
{
    if (index < 0 || (guint)index >= menu->cells->len) return;
    MenuCell *cell = g_ptr_array_index(menu->cells, (guint)index);
    if (cell->capability->state != FIREMOD_ACTION_AVAILABLE) return;
    gchar *id = g_strdup(cell->capability->id);
    gdouble x = menu->global_x, y = menu->global_y;
    firemod_menu_close(menu);
    GdkDisplay *display = gdk_display_get_default();
    if (display != NULL) gdk_display_flush(display);
    menu->action_request(id, x, y, menu->user_data);
    g_free(id);
}

static void primary_pressed(
    GtkGestureClick *gesture,
    gint presses,
    gdouble x,
    gdouble y,
    gpointer user_data)
{
    (void)presses;
    FiremodMenu *menu = user_data;
    if (!menu->armed) return;
    gint index = hit_test(menu, x, y);
    if (index < 0) {
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        firemod_menu_close(menu);
        return;
    }
    menu->focused = index;
    gtk_widget_queue_draw(GTK_WIDGET(menu->canvas));
    dispatch_index(menu, index);
}

static gint next_direction(FiremodMenu *menu, guint keyval)
{
    if (menu->cells->len == 0) return -1;
    gint current = CLAMP(menu->focused, 0, (gint)menu->cells->len - 1);
    MenuCell *from = g_ptr_array_index(menu->cells, (guint)current);
    gint best = current;
    gdouble best_score = G_MAXDOUBLE;
    for (guint i = 0; i < menu->cells->len; i++) {
        if ((gint)i == current) continue;
        MenuCell *to = g_ptr_array_index(menu->cells, i);
        gdouble dx = to->x - from->x, dy = to->y - from->y;
        gboolean valid = (keyval == GDK_KEY_Left && dx < -1) ||
            (keyval == GDK_KEY_Right && dx > 1) ||
            (keyval == GDK_KEY_Up && dy < -1) ||
            (keyval == GDK_KEY_Down && dy > 1);
        if (!valid) continue;
        gdouble score = dx * dx + dy * dy;
        if (score < best_score) { best_score = score; best = (gint)i; }
    }
    return best;
}

static gboolean menu_key(
    GtkEventControllerKey *controller,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    (void)controller; (void)keycode;
    FiremodMenu *menu = user_data;
    if (keyval == GDK_KEY_Escape) { firemod_menu_close(menu); return TRUE; }
    if (menu->cells->len == 0) return FALSE;
    if (keyval == GDK_KEY_Tab) {
        gint delta = (state & GDK_SHIFT_MASK) ? -1 : 1;
        menu->focused = (menu->focused + delta + (gint)menu->cells->len) %
            (gint)menu->cells->len;
    } else if (keyval == GDK_KEY_Left || keyval == GDK_KEY_Right ||
               keyval == GDK_KEY_Up || keyval == GDK_KEY_Down) {
        menu->focused = next_direction(menu, keyval);
    } else if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter ||
               keyval == GDK_KEY_space) {
        dispatch_index(menu, menu->focused);
        return TRUE;
    } else return FALSE;
    update_accessible_action(menu, menu->focused);
    gtk_widget_queue_draw(GTK_WIDGET(menu->canvas));
    return TRUE;
}

static gboolean arm_menu(gpointer user_data)
{
    FiremodMenu *menu = user_data;
    menu->arm_timer = 0;
    menu->armed = TRUE;
    return G_SOURCE_REMOVE;
}

static void add_cell(
    FiremodMenu *menu,
    FiremodCapability *capability,
    FiremodHexAxial axial,
    guint order)
{
    FiremodHexPoint point = firemod_hex_axial_to_pixel(
        axial, CELL_WIDTH * menu->scale, CELL_HEIGHT * menu->scale);
    MenuCell *cell = g_new0(MenuCell, 1);
    cell->capability = capability;
    cell->x = point.x;
    cell->y = point.y;
    cell->order = order;
    g_ptr_array_add(menu->cells, cell);
}

static void collect_cells(FiremodMenu *menu)
{
    g_ptr_array_set_size(menu->cells, 0);
    for (gint slot = 0; slot < 7; slot++) {
        for (guint i = 0; i < menu->registry->items->len; i++) {
            FiremodCapability *capability = g_ptr_array_index(
                menu->registry->items, i);
            if (capability->canonical_slot == slot && capability->menu_visible) {
                add_cell(menu, capability, firemod_hex_canonical_axial(slot),
                    (guint)slot);
                break;
            }
        }
    }
    GPtrArray *outer = g_ptr_array_new();
    for (guint i = 0; i < menu->registry->items->len; i++) {
        FiremodCapability *capability = g_ptr_array_index(menu->registry->items, i);
        if (capability->canonical_slot >= 0 || !capability->menu_visible ||
            capability->state == FIREMOD_ACTION_HIDDEN ||
            capability->state == FIREMOD_ACTION_REJECTED) continue;
        if (capability->origin == FIREMOD_ORIGIN_EXTERNAL_MODULE &&
            capability->state != FIREMOD_ACTION_AVAILABLE) continue;
        g_ptr_array_add(outer, capability);
    }
    g_ptr_array_sort(outer, capability_order);
    for (guint i = 0; i < outer->len; i++) {
        add_cell(menu, g_ptr_array_index(outer, i),
            firemod_hex_outer_axial(i), 7 + i);
    }
    g_ptr_array_free(outer, TRUE);
}

static void fit_cells(FiremodMenu *menu, gint width, gint height)
{
    gdouble requested = CLAMP(menu->settings->active->menu_scale, 0.5, 1.0);
    menu->scale = requested;
    while (TRUE) {
        collect_cells(menu);
        if (menu->cells->len == 0) return;
        gdouble min_x = G_MAXDOUBLE, min_y = G_MAXDOUBLE;
        gdouble max_x = -G_MAXDOUBLE, max_y = -G_MAXDOUBLE;
        gdouble half_w = CELL_WIDTH * menu->scale * 0.5;
        gdouble half_h = CELL_HEIGHT * menu->scale * 0.5;
        for (guint i = 0; i < menu->cells->len; i++) {
            MenuCell *cell = g_ptr_array_index(menu->cells, i);
            min_x = MIN(min_x, cell->x - half_w); max_x = MAX(max_x, cell->x + half_w);
            min_y = MIN(min_y, cell->y - half_h); max_y = MAX(max_y, cell->y + half_h);
        }
        gdouble fit = MIN((width - SAFE_MARGIN * 2) / (max_x - min_x),
            (height - SAFE_MARGIN * 2) / (max_y - min_y));
        if (fit >= 1.0 || menu->scale <= 0.5) break;
        menu->scale = MAX(0.5, menu->scale * fit);
    }
    gdouble min_x = G_MAXDOUBLE, min_y = G_MAXDOUBLE;
    gdouble max_x = -G_MAXDOUBLE, max_y = -G_MAXDOUBLE;
    gdouble half_w = CELL_WIDTH * menu->scale * 0.5;
    gdouble half_h = CELL_HEIGHT * menu->scale * 0.5;
    for (guint i = 0; i < menu->cells->len; i++) {
        MenuCell *cell = g_ptr_array_index(menu->cells, i);
        min_x = MIN(min_x, cell->x - half_w); max_x = MAX(max_x, cell->x + half_w);
        min_y = MIN(min_y, cell->y - half_h); max_y = MAX(max_y, cell->y + half_h);
    }
    gdouble origin_x = menu->global_x - menu->monitor_geometry.x;
    gdouble origin_y = menu->global_y - menu->monitor_geometry.y;
    origin_x = CLAMP(origin_x, SAFE_MARGIN - min_x, width - SAFE_MARGIN - max_x);
    origin_y = CLAMP(origin_y, SAFE_MARGIN - min_y, height - SAFE_MARGIN - max_y);
    for (guint i = 0; i < menu->cells->len; i++) {
        MenuCell *cell = g_ptr_array_index(menu->cells, i);
        cell->x += origin_x; cell->y += origin_y;
    }
}

static gboolean close_requested(GtkWindow *window, gpointer user_data)
{
    (void)window;
    FiremodMenu *menu = user_data;
    menu->window = NULL; menu->canvas = NULL;
    g_ptr_array_set_size(menu->cells, 0);
    return FALSE;
}

void firemod_menu_close(FiremodMenu *menu)
{
    if (menu == NULL) return;
    if (menu->hover_timer != 0) g_source_remove(menu->hover_timer);
    if (menu->arm_timer != 0) g_source_remove(menu->arm_timer);
    menu->hover_timer = menu->arm_timer = 0;
    menu->hovered = menu->focused = menu->pending_hover = -1;
    menu->hover_visible = FALSE; menu->armed = FALSE;
    if (menu->window != NULL) gtk_window_destroy(menu->window);
    menu->window = NULL; menu->canvas = NULL;
    g_ptr_array_set_size(menu->cells, 0);
}

void firemod_menu_show_at(FiremodMenu *menu, double global_x, double global_y)
{
    g_return_if_fail(menu != NULL);
    if (!menu->settings->active->menu_enabled) return;
    if (menu->window != NULL) { firemod_menu_close(menu); return; }
    GdkMonitor *monitor = monitor_at(gdk_display_get_default(), global_x, global_y);
    if (monitor == NULL || !gtk_layer_is_supported()) return;
    gdk_monitor_get_geometry(monitor, &menu->monitor_geometry);
    menu->global_x = global_x; menu->global_y = global_y;
    fit_cells(menu, menu->monitor_geometry.width, menu->monitor_geometry.height);
    menu->hovered = -1; menu->focused = menu->cells->len > 0 ? 0 : -1;
    GtkWindow *window = GTK_WINDOW(gtk_application_window_new(menu->application));
    menu->window = window;
    g_signal_connect(window, "close-request", G_CALLBACK(close_requested), menu);
    gtk_widget_add_css_class(GTK_WIDGET(window), "hex-utility-overlay");
    gtk_window_set_decorated(window, FALSE);
    gtk_layer_init_for_window(window);
    gtk_layer_set_namespace(window, "firemod-hex-menu");
    gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_monitor(window, monitor);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    g_object_unref(monitor);
    menu->canvas = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_widget_set_focusable(GTK_WIDGET(menu->canvas), TRUE);
    gtk_drawing_area_set_draw_func(menu->canvas, draw_menu, menu, NULL);
    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(motion), menu);
    g_signal_connect(motion_controller, "leave", G_CALLBACK(leave), menu);
    gtk_widget_add_controller(GTK_WIDGET(menu->canvas), motion_controller);
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    g_signal_connect(click, "pressed", G_CALLBACK(primary_pressed), menu);
    gtk_widget_add_controller(GTK_WIDGET(menu->canvas), GTK_EVENT_CONTROLLER(click));
    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(menu_key), menu);
    gtk_widget_add_controller(GTK_WIDGET(menu->canvas), keys);
    gtk_window_set_child(window, GTK_WIDGET(menu->canvas));
    gtk_window_present(window);
    update_accessible_action(menu, menu->focused);
    gtk_widget_grab_focus(GTK_WIDGET(menu->canvas));
    menu->armed = FALSE;
    menu->arm_timer = g_timeout_add(120, arm_menu, menu);
}

FiremodMenu *firemod_menu_new(
    GtkApplication *application,
    FiremodCapabilityRegistry *registry,
    FiremodSettings *settings,
    FiremodActionRequest action_request,
    gpointer user_data)
{
    FiremodMenu *menu = g_new0(FiremodMenu, 1);
    menu->application = application; menu->registry = registry;
    menu->settings = settings; menu->action_request = action_request;
    menu->user_data = user_data;
    menu->cells = g_ptr_array_new_with_free_func((GDestroyNotify)cell_free);
    menu->assets = firemod_hex_assets_new();
    menu->hovered = menu->focused = menu->pending_hover = -1;
    return menu;
}

void firemod_menu_free(FiremodMenu *menu)
{
    if (menu == NULL) return;
    firemod_menu_close(menu);
    firemod_hex_assets_free(menu->assets);
    g_ptr_array_free(menu->cells, TRUE);
    g_free(menu);
}
