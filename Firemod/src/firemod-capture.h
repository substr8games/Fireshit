#ifndef FIREMOD_CAPTURE_H
#define FIREMOD_CAPTURE_H

#include <gtk/gtk.h>

typedef struct FiremodApp FiremodApp;

void firemod_capture_output(FiremodApp *app, double global_x, double global_y);
void firemod_capture_region(FiremodApp *app, double global_x, double global_y);

#endif
