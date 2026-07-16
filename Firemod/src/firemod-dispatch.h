#ifndef FIREMOD_DISPATCH_H
#define FIREMOD_DISPATCH_H

#include "firemod-app.h"

void firemod_dispatch_action(
    const gchar *capability_id,
    double global_x,
    double global_y,
    gpointer user_data);

#endif
