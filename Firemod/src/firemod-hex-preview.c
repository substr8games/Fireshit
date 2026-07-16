#include "firemod-hex-preview.h"

#include "firemod-hex-assets.h"
#include "firemod-hex-layout.h"

#define PREVIEW_CELL_W FIREMOD_HEX_CELL_WIDTH
#define PREVIEW_CELL_H FIREMOD_HEX_CELL_HEIGHT
#define PREVIEW_ICON FIREMOD_HEX_ICON_MAX

typedef struct {
    FiremodCapabilityRegistry *registry;
    FiremodSettings *settings;
    FiremodHexAssets *assets;
} PreviewState;

static void draw_hex(
    PreviewState *state,
    cairo_t *cr,
    FiremodCapability *capability,
    FiremodHexAxial axial,
    gdouble origin_x,
    gdouble origin_y)
{
    FiremodHexPoint point = firemod_hex_axial_to_pixel(
        axial, PREVIEW_CELL_W, PREVIEW_CELL_H);
    gdouble x = origin_x + point.x;
    gdouble y = origin_y + point.y;
    GdkRGBA accent = {0.0, 0.27, 0.67, 1.0};
    if (capability->color != NULL) gdk_rgba_parse(&accent, capability->color);
    firemod_hex_path(cr, x, y, PREVIEW_CELL_W, PREVIEW_CELL_H, 1.0);
    cairo_set_source_rgba(cr, accent.red, accent.green, accent.blue, 0.26);
    cairo_fill_preserve(cr);
    GdkRGBA border;
    gdk_rgba_parse(&border, capability->state == FIREMOD_ACTION_AVAILABLE ?
        "#26333d" : "#ff7474");
    cairo_set_source_rgba(cr, border.red, border.green, border.blue, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    firemod_hex_assets_draw(state->assets, cr, capability,
        x, y, PREVIEW_ICON, 1.0);
}

static void draw_preview(
    GtkDrawingArea *area,
    cairo_t *cr,
    gint width,
    gint height,
    gpointer user_data)
{
    (void)area;
    PreviewState *state = user_data;
    gdouble origin_x = width * 0.5;
    gdouble origin_y = height * 0.5;
    for (guint slot = 0; slot < FIREMOD_CANONICAL_COUNT; slot++) {
        if (!state->settings->draft->canonical_visible[slot]) continue;
        FiremodCapability *capability = firemod_capability_find(
            state->registry, firemod_settings_canonical_id(slot));
        if (capability != NULL) draw_hex(state, cr, capability,
            firemod_hex_canonical_axial((gint)slot), origin_x, origin_y);
    }
    for (guint i = 0, visible = 0;
         i < state->settings->draft->actions->len; i++) {
        FiremodCustomAction *action = g_ptr_array_index(
            state->settings->draft->actions, i);
        if (!action->enabled) continue;
        FiremodCapability capability = {0};
        capability.id = action->id;
        capability.label = action->label;
        capability.icon_path = action->icon_path;
        capability.icon_resource =
            "/org/philabs/Firemod/icons/utility-menu/run-script.png";
        capability.color = action->color;
        draw_hex(state, cr, &capability, firemod_hex_outer_axial(visible++),
            origin_x, origin_y);
    }
}

static void preview_free(gpointer data)
{
    PreviewState *state = data;
    firemod_hex_assets_free(state->assets);
    g_free(state);
}

GtkWidget *firemod_hex_preview_new(
    FiremodCapabilityRegistry *registry,
    FiremodSettings *settings)
{
    PreviewState *state = g_new0(PreviewState, 1);
    state->registry = registry;
    state->settings = settings;
    state->assets = firemod_hex_assets_new();
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), 520);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), 300);
    gtk_widget_add_css_class(area, "hex-preview");
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area),
        draw_preview, state, preview_free);
    return area;
}

void firemod_hex_preview_refresh(GtkWidget *preview)
{
    if (preview != NULL) gtk_widget_queue_draw(preview);
}
