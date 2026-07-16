#ifndef FIREMOD_SCREENCOPY_PRIVATE_H
#define FIREMOD_SCREENCOPY_PRIVATE_H

#include "firemod-screencopy.h"

#include <wayland-client.h>

#include "wlr-screencopy-unstable-v1-client-protocol.h"

struct FiremodScreencopy {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_shm *shm;
    struct zwlr_screencopy_manager_v1 *manager;
    GPtrArray *pending;
};

void firemod_screencopy_frame_cancel_all(FiremodScreencopy *copy);

gboolean firemod_screencopy_frame_begin(
    FiremodScreencopy *copy,
    struct wl_output *output,
    FiremodCaptureCallback callback,
    gpointer user_data,
    GError **error);

#endif
