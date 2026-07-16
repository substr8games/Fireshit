#ifndef FIREMOD_AUTHORITY_WIDGET_H
#define FIREMOD_AUTHORITY_WIDGET_H

#include <gtk/gtk.h>

#include "firemod-model.h"

GtkWidget *firemod_authority_widget_new(FiremodAuthority *authority);
gboolean firemod_authority_matches(FiremodAuthority *authority, const char *query);
gboolean firemod_text_matches(const char *text, const char *query);

#endif
