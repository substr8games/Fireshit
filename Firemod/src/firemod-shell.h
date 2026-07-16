#ifndef FIREMOD_SHELL_H
#define FIREMOD_SHELL_H

#include "firemod-app.h"

void firemod_shell_install_css(void);
void firemod_shell_build(FiremodApp *app);
void firemod_shell_select_page(FiremodApp *app, const gchar *page);

#endif
