#ifndef FIREMOD_SCREENCOPY_H
#define FIREMOD_SCREENCOPY_H

#include <gtk/gtk.h>

typedef struct FiremodScreencopy FiremodScreencopy;

typedef struct {
    guint width;
    guint height;
    guint stride;
    GBytes *rgba;
} FiremodCaptureFrame;

typedef void (*FiremodCaptureCallback)(
    FiremodCaptureFrame *frame,
    const GError *error,
    gpointer user_data);

FiremodScreencopy *firemod_screencopy_new(
    GdkDisplay *display,
    GError **error);
void firemod_screencopy_free(FiremodScreencopy *copy);
gboolean firemod_screencopy_capture(
    FiremodScreencopy *copy,
    GdkMonitor *monitor,
    FiremodCaptureCallback callback,
    gpointer user_data,
    GError **error);
void firemod_capture_frame_free(FiremodCaptureFrame *frame);

#endif
