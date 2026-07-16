#ifndef FIREMOD_ACTION_EDITOR_H
#define FIREMOD_ACTION_EDITOR_H

#include <gtk/gtk.h>

#include "firemod-settings.h"

typedef void (*FiremodActionEditorDone)(
    FiremodCustomAction *action,
    gpointer user_data);

void firemod_action_editor_show(
    GtkWindow *parent,
    const FiremodCustomAction *source,
    FiremodCustomType type,
    FiremodActionEditorDone done,
    gpointer user_data);

#endif
