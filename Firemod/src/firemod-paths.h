#ifndef FIREMOD_PATHS_H
#define FIREMOD_PATHS_H

#include <glib.h>

char *firemod_find_wayfire_config(void);
char *firemod_expand_path(const char *value);
char *firemod_normalize_token(const char *value);
gboolean firemod_path_is_ignored(const char *path);
gboolean firemod_path_is_regular_config(const char *path);

#endif
