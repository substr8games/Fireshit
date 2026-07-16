#include "firemod-discovery.h"

#include "firemod-first-party.h"
#include "firemod-registry.h"
#include "firemod-xdg.h"

static gint compare_modules(gconstpointer left, gconstpointer right)
{
    const FiremodModule *a = *(FiremodModule * const *)left;
    const FiremodModule *b = *(FiremodModule * const *)right;
    if (a->first_party != b->first_party) {
        return a->first_party ? -1 : 1;
    }
    return g_utf8_collate(a->name, b->name);
}

FiremodInventory *firemod_discovery_scan(void)
{
    FiremodInventory *inventory = firemod_inventory_new();
    firemod_registry_register(inventory);
    firemod_first_party_discover(inventory);
    firemod_xdg_discover(inventory);
    g_ptr_array_sort(inventory->modules, compare_modules);
    g_ptr_array_sort(inventory->uncertain_modules, compare_modules);
    return inventory;
}
