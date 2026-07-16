#ifndef FIREMOD_ACTIONS_H
#define FIREMOD_ACTIONS_H

#include <gtk/gtk.h>

void firemod_action_open_path(GtkWindow *parent, const char *path);
void firemod_action_open_folder(GtkWindow *parent, const char *path);
void firemod_action_copy_path(GtkWidget *widget, const char *path);
void firemod_action_run(GtkWindow *parent, const char *command);

#endif
