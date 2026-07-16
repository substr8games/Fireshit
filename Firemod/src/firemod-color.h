#ifndef FIREMOD_COLOR_H
#define FIREMOD_COLOR_H

#include <glib.h>

typedef struct {
    guint8 red;
    guint8 green;
    guint8 blue;
    gchar *hex;
    gchar *rgb;
    gchar *hsl;
    gchar *oklch;
} FiremodColor;

FiremodColor *firemod_color_new(guint8 red, guint8 green, guint8 blue);
void firemod_color_free(FiremodColor *color);

#endif
