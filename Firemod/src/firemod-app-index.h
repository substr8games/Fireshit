#ifndef FIREMOD_APP_INDEX_H
#define FIREMOD_APP_INDEX_H

#include <gio/gio.h>

typedef struct _FiremodAppCandidate FiremodAppCandidate;

GPtrArray *firemod_app_index_load(void);
FiremodAppCandidate *firemod_app_index_match(
    GPtrArray *candidates,
    const char *base);
GAppInfo *firemod_app_candidate_info(FiremodAppCandidate *candidate);

#endif
