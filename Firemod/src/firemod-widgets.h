#ifndef FIREMOD_WIDGETS_H
#define FIREMOD_WIDGETS_H

#include "firemod-app.h"

GtkWidget *firemod_widget_badge(const char *text, const char *css_class);
GtkWidget *firemod_widget_module_card(
    FiremodApp *app,
    FiremodModule *module,
    gboolean settings_only,
    const char *query);
GtkWidget *firemod_widget_empty(const char *title, const char *detail);

gboolean firemod_module_matches(
    FiremodModule *module,
    FiremodFilter filter,
    const char *query);

#endif
