#ifndef FIREMOD_RUNTIME_H
#define FIREMOD_RUNTIME_H

#include <gio/gio.h>

typedef struct FiremodRuntime FiremodRuntime;
typedef void (*FiremodMenuRequest)(double x, double y, gpointer user_data);

FiremodRuntime *firemod_runtime_new(
    FiremodMenuRequest menu_request,
    gpointer user_data,
    GError **error);
void firemod_runtime_free(FiremodRuntime *runtime);
const gchar *firemod_runtime_path(FiremodRuntime *runtime);

#endif
