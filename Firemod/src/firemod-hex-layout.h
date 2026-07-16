#ifndef FIREMOD_HEX_LAYOUT_H
#define FIREMOD_HEX_LAYOUT_H

#include <cairo.h>
#include <glib.h>

#define FIREMOD_HEX_CELL_WIDTH 88.0
#define FIREMOD_HEX_CELL_HEIGHT 76.0
#define FIREMOD_HEX_ICON_MAX 64.0

typedef struct {
    gint q;
    gint r;
} FiremodHexAxial;

typedef struct {
    gdouble x;
    gdouble y;
} FiremodHexPoint;

FiremodHexAxial firemod_hex_canonical_axial(gint slot);
FiremodHexAxial firemod_hex_outer_axial(guint outer_index);
FiremodHexPoint firemod_hex_axial_to_pixel(
    FiremodHexAxial axial,
    gdouble cell_width,
    gdouble cell_height);
gboolean firemod_hex_contains(
    gdouble point_x,
    gdouble point_y,
    gdouble center_x,
    gdouble center_y,
    gdouble width,
    gdouble height);
void firemod_hex_path(
    cairo_t *cr,
    gdouble center_x,
    gdouble center_y,
    gdouble width,
    gdouble height,
    gdouble inset);

#endif
