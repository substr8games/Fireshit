#ifndef FIREMOD_CAPTURE_PRIVATE_H
#define FIREMOD_CAPTURE_PRIVATE_H

#include "firemod-capture.h"

#include "firemod-app.h"
#include "firemod-screencopy.h"

typedef enum {
    FIREMOD_CAPTURE_OUTPUT = 0,
    FIREMOD_CAPTURE_REGION,
} FiremodCaptureMode;

typedef struct {
    FiremodApp *app;
    FiremodCaptureMode mode;
    GdkMonitor *monitor;
    GdkRectangle geometry;
    FiremodCaptureFrame *frame;
    GtkWindow *window;
    gboolean dragging;
    double start_x;
    double start_y;
    double end_x;
    double end_y;
} FiremodCaptureRequest;

void firemod_capture_request_free(FiremodCaptureRequest *request);
void firemod_capture_report_error(const gchar *detail);
gboolean firemod_capture_save_region(
    FiremodCaptureRequest *request,
    double logical_x,
    double logical_y,
    double logical_width,
    double logical_height,
    gchar **saved_path);
void firemod_capture_show_selector(FiremodCaptureRequest *request);

#endif
