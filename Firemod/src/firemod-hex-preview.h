#ifndef FIREMOD_HEX_PREVIEW_H
#define FIREMOD_HEX_PREVIEW_H

#include <gtk/gtk.h>

#include "firemod-capability.h"
#include "firemod-settings.h"

GtkWidget *firemod_hex_preview_new(
    FiremodCapabilityRegistry *registry,
    FiremodSettings *settings);
void firemod_hex_preview_refresh(GtkWidget *preview);

#endif
