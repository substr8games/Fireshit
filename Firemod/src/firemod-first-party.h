#ifndef FIREMOD_FIRST_PARTY_H
#define FIREMOD_FIRST_PARTY_H

#include "firemod-capability.h"
#include "firemod-model.h"

void firemod_first_party_discover(FiremodInventory *inventory);
void firemod_first_party_attach_capabilities(
    FiremodInventory *inventory,
    FiremodCapabilityRegistry *registry);

#endif
