#ifndef FIREMOD_PARSE_H
#define FIREMOD_PARSE_H

#include "firemod-model.h"

FiremodAuthorityType firemod_parse_type_for_path(const char *path);
void firemod_parse_authority(FiremodAuthority *authority);

#endif
