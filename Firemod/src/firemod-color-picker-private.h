#ifndef FIREMOD_COLOR_PICKER_PRIVATE_H
#define FIREMOD_COLOR_PICKER_PRIVATE_H

#include "firemod-color-picker.h"
#include "firemod-color.h"

struct FiremodColorPicker {
    GtkApplication *application;
    FiremodScreencopy *screencopy;
    GtkWindow *window;
    GtkWindow *result_window;
    GdkMonitor *monitor;
    GdkRectangle geometry;
    FiremodCaptureFrame *frame;
    double pointer_x;
    double pointer_y;
    gboolean capture_pending;
};

void firemod_picker_view_show(FiremodColorPicker *picker);
void firemod_picker_view_close(FiremodColorPicker *picker);
void firemod_picker_accept(FiremodColorPicker *picker);
void firemod_picker_result_show(
    FiremodColorPicker *picker,
    FiremodColor *color);
void firemod_picker_cancel(FiremodColorPicker *picker);
void firemod_picker_sample(
    FiremodColorPicker *picker,
    double x,
    double y,
    guint8 *red,
    guint8 *green,
    guint8 *blue);

#endif
