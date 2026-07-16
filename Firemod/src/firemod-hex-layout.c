#include "firemod-hex-layout.h"

#include <cairo.h>
#include <math.h>

FiremodHexAxial firemod_hex_canonical_axial(gint slot)
{
    static const FiremodHexAxial positions[7] = {
        {0, 0}, {0, -1}, {1, -1}, {1, 0},
        {0, 1}, {-1, 1}, {-1, 0}
    };
    if (slot < 0 || slot > 6) return (FiremodHexAxial){0, 0};
    return positions[slot];
}

static guint ring_for_index(guint index, guint *offset)
{
    guint ring = 2;
    guint start = 0;
    while (index >= start + 6 * ring) {
        start += 6 * ring;
        ring++;
    }
    if (offset != NULL) *offset = index - start;
    return ring;
}

FiremodHexAxial firemod_hex_outer_axial(guint outer_index)
{
    guint offset = 0;
    guint ring = ring_for_index(outer_index, &offset);
    static const FiremodHexAxial directions[6] = {
        {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {0, -1}, {1, -1}
    };
    FiremodHexAxial point = {0, -(gint)ring};
    guint side = offset / ring;
    guint step = offset % ring;
    for (guint i = 0; i < side; i++) {
        point.q += directions[i].q * (gint)ring;
        point.r += directions[i].r * (gint)ring;
    }
    point.q += directions[side].q * (gint)step;
    point.r += directions[side].r * (gint)step;
    return point;
}

FiremodHexPoint firemod_hex_axial_to_pixel(
    FiremodHexAxial axial,
    gdouble cell_width,
    gdouble cell_height)
{
    return (FiremodHexPoint){
        .x = cell_width * 0.75 * axial.q,
        .y = cell_height * (axial.r + axial.q * 0.5)
    };
}

gboolean firemod_hex_contains(
    gdouble point_x,
    gdouble point_y,
    gdouble center_x,
    gdouble center_y,
    gdouble width,
    gdouble height)
{
    gdouble x = fabs(point_x - center_x) / (width * 0.5);
    gdouble y = fabs(point_y - center_y) / (height * 0.5);
    if (x > 1.0 || y > 1.0) return FALSE;
    return x <= 0.5 || y <= 2.0 * (1.0 - x);
}

void firemod_hex_path(
    cairo_t *cr,
    gdouble center_x,
    gdouble center_y,
    gdouble width,
    gdouble height,
    gdouble inset)
{
    gdouble half_w = MAX(1.0, width * 0.5 - inset);
    gdouble half_h = MAX(1.0, height * 0.5 - inset);
    cairo_move_to(cr, center_x - half_w * 0.5, center_y - half_h);
    cairo_line_to(cr, center_x + half_w * 0.5, center_y - half_h);
    cairo_line_to(cr, center_x + half_w, center_y);
    cairo_line_to(cr, center_x + half_w * 0.5, center_y + half_h);
    cairo_line_to(cr, center_x - half_w * 0.5, center_y + half_h);
    cairo_line_to(cr, center_x - half_w, center_y);
    cairo_close_path(cr);
}
