#ifndef FIREMOD_HEX_ASSETS_H
#define FIREMOD_HEX_ASSETS_H

#include <cairo.h>

#include "firemod-capability.h"

typedef struct FiremodHexAssets FiremodHexAssets;

FiremodHexAssets *firemod_hex_assets_new(void);
void firemod_hex_assets_free(FiremodHexAssets *assets);
gboolean firemod_hex_assets_draw(
    FiremodHexAssets *assets,
    cairo_t *cr,
    const FiremodCapability *capability,
    gdouble center_x,
    gdouble center_y,
    gdouble maximum,
    gdouble opacity);

#endif
