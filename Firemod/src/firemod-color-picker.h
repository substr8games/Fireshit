#ifndef FIREMOD_COLOR_PICKER_H
#define FIREMOD_COLOR_PICKER_H

#include <gtk/gtk.h>

#include "firemod-screencopy.h"

typedef struct FiremodColorPicker FiremodColorPicker;

FiremodColorPicker *firemod_color_picker_new(
    GtkApplication *application,
    FiremodScreencopy *screencopy);
void firemod_color_picker_start(
    FiremodColorPicker *picker,
    double global_x,
    double global_y);
void firemod_color_picker_free(FiremodColorPicker *picker);

#endif
