#ifndef FIREMOD_SETTINGS_PRIVATE_H
#define FIREMOD_SETTINGS_PRIVATE_H

#include "firemod-settings.h"

FiremodSettingsState *firemod_settings_state_new_default(void);
FiremodSettingsState *firemod_settings_state_clone(
    const FiremodSettingsState *source);
void firemod_settings_state_free(FiremodSettingsState *state);
GKeyFile *firemod_settings_state_to_key_file(
    const FiremodSettingsState *state);
FiremodSettingsState *firemod_settings_state_from_key_file(
    GKeyFile *key,
    GError **error);

#endif
