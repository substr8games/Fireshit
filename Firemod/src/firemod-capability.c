#include "firemod-capability.h"

#include "firemod-build-config.h"

#include <string.h>

#define FIREMOD_CAPABILITY_CONTRACT_VERSION 1

static FiremodCapability *capability_new(
    const char *id,
    const char *owner,
    const char *label,
    const char *icon,
    gint order)
{
    FiremodCapability *item = g_new0(FiremodCapability, 1);
    item->id = g_strdup(id);
    item->owner = g_strdup(owner);
    item->label = g_strdup(label);
    item->short_label = g_ascii_strup(label, -1);
    item->summary = g_strdup(owner);
    item->icon_name = g_strdup(icon);
    item->color = g_strdup("#0046aa");
    item->menu_group = g_strdup("fireshit");
    item->menu_order = order;
    item->canonical_slot = -1;
    item->menu_visible = TRUE;
    item->state = FIREMOD_ACTION_UNAVAILABLE;
    item->origin = FIREMOD_ORIGIN_CANONICAL;
    return item;
}

static void capability_free(FiremodCapability *item)
{
    if (item == NULL) return;
    g_free(item->id);
    g_free(item->owner);
    g_free(item->label);
    g_free(item->short_label);
    g_free(item->summary);
    g_free(item->icon_name);
    g_free(item->icon_resource);
    g_free(item->icon_path);
    g_free(item->color);
    g_free(item->menu_group);
    g_free(item->availability_reason);
    g_free(item->executable);
    g_free(item->command);
    g_strfreev(item->argv);
    g_free(item->working_directory);
    g_free(item);
}

void firemod_capability_set_state(
    FiremodCapability *capability,
    FiremodActionState state,
    const gchar *reason)
{
    g_return_if_fail(capability != NULL);
    capability->state = state;
    g_free(capability->availability_reason);
    capability->availability_reason = g_strdup(reason);
}

static void replace_text(gchar **field, const gchar *value)
{
    g_free(*field);
    *field = g_strdup(value);
}

static void set_builtin_visual(
    FiremodCapability *item,
    gint slot,
    const gchar *resource,
    const gchar *short_label,
    const gchar *summary,
    const gchar *color)
{
    item->canonical_slot = slot;
    replace_text(&item->icon_resource, resource);
    replace_text(&item->short_label, short_label);
    replace_text(&item->summary, summary);
    replace_text(&item->color, color);
}

static void add_builtin(
    FiremodCapabilityRegistry *registry,
    const char *id,
    const char *owner,
    const char *label,
    const char *icon,
    gint order)
{
    g_ptr_array_add(registry->items,
        capability_new(id, owner, label, icon, order));
}

static void register_builtins(FiremodCapabilityRegistry *registry)
{
    add_builtin(registry, "core.settings.open", "Firemod Core",
        "Firemod Settings", "preferences-system-symbolic", 0);
    add_builtin(registry, "capture.screenshot.output", "Firemod Capture",
        "Screenshot", "camera-photo-symbolic", 10);
    add_builtin(registry, "capture.screenshot.region", "Firemod Capture",
        "Screen Snip", "edit-cut-symbolic", 20);
    add_builtin(registry, "capture.record.output", "FrameTap",
        "Record Screen", "media-record-symbolic", 30);
    add_builtin(registry, "capture.record.application", "FrameTap",
        "Record Application", "applications-multimedia-symbolic", 40);
    add_builtin(registry, "capture.record.region", "FrameTap",
        "Record Region", "selection-rectangular-symbolic", 50);
    add_builtin(registry, "utility.color.pick", "Firemod Color Picker",
        "Color Picker", "color-select-symbolic", 60);

    set_builtin_visual(g_ptr_array_index(registry->items, 0), 0,
        "/org/philabs/Firemod/icons/utility-menu/fireshit-color.png",
        "FIREMOD", "Open dedicated Fireshit settings", "#fb3b02");
    set_builtin_visual(g_ptr_array_index(registry->items, 1), 1,
        "/org/philabs/Firemod/icons/utility-menu/camera-screenshot.png",
        "SCREENSHOT", "Capture the active output", "#ffea0c");
    set_builtin_visual(g_ptr_array_index(registry->items, 2), 2,
        "/org/philabs/Firemod/icons/utility-menu/camera-screensnip.png",
        "SCREEN SNIP", "Capture a selected region", "#ffea0c");
    set_builtin_visual(g_ptr_array_index(registry->items, 3), 4,
        "/org/philabs/Firemod/icons/utility-menu/record-screen.png",
        "RECORD SCREEN", "Record the active output", "#d0000c");
    set_builtin_visual(g_ptr_array_index(registry->items, 4), 5,
        "/org/philabs/Firemod/icons/utility-menu/record-application.png",
        "RECORD APP", "Record the focused application", "#d0000c");
    set_builtin_visual(g_ptr_array_index(registry->items, 5), 3,
        "/org/philabs/Firemod/icons/utility-menu/record-region.png",
        "RECORD REGION", "Record a selected region", "#d0000c");
    set_builtin_visual(g_ptr_array_index(registry->items, 6), 6,
        "/org/philabs/Firemod/icons/utility-menu/color-picker-droper.png",
        "COLOR PICKER", "Sample a screen color and copy HEX", "#00ffff");

    firemod_capability_set_state(g_ptr_array_index(registry->items, 0),
        FIREMOD_ACTION_AVAILABLE, "Core runtime available");
    for (guint i = 1; i < registry->items->len; i++) {
        firemod_capability_set_state(g_ptr_array_index(registry->items, i),
            FIREMOD_ACTION_UNAVAILABLE, "Owning capability is not registered");
    }
    FiremodCapability *frametap = g_ptr_array_index(registry->items, 4);
    frametap->executable = g_find_program_in_path("frametap");
    firemod_capability_set_state(frametap,
        frametap->executable != NULL ?
            FIREMOD_ACTION_AVAILABLE : FIREMOD_ACTION_UNAVAILABLE,
        frametap->executable != NULL ?
            "FrameTap command interface available" : "FrameTap is not installed");
}

FiremodCapability *firemod_capability_find(
    FiremodCapabilityRegistry *registry,
    const gchar *id)
{
    if (registry == NULL || id == NULL) return NULL;
    for (guint i = 0; i < registry->items->len; i++) {
        FiremodCapability *item = g_ptr_array_index(registry->items, i);
        if (g_strcmp0(item->id, id) == 0) return item;
    }
    return NULL;
}

static gchar *find_executable(const char *executable)
{
    if (executable == NULL || *executable == '\0') return NULL;
    if (strchr(executable, G_DIR_SEPARATOR) != NULL) {
        return g_file_test(executable, G_FILE_TEST_IS_EXECUTABLE) ?
            g_canonicalize_filename(executable, NULL) : NULL;
    }
    return g_find_program_in_path(executable);
}

static void apply_manifest(
    FiremodCapabilityRegistry *registry,
    const char *path)
{
    GKeyFile *key = g_key_file_new();
    if (!g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, NULL) ||
        !g_key_file_has_group(key, "capability") ||
        g_key_file_get_integer(key, "capability", "contract-version", NULL) !=
            FIREMOD_CAPABILITY_CONTRACT_VERSION) {
        g_key_file_unref(key);
        return;
    }
    char *id = g_key_file_get_string(key, "capability", "id", NULL);
    char *owner = g_key_file_get_string(key, "capability", "owner", NULL);
    char *label = g_key_file_get_string(key, "capability", "label", NULL);
    char *icon = g_key_file_get_string(key, "capability", "icon", NULL);
    char *command = g_key_file_get_string(key, "capability", "command", NULL);
    char *executable = g_key_file_get_string(key, "capability", "executable", NULL);
    gboolean visible = g_key_file_get_boolean(
        key, "capability", "menu-visible", NULL);
    if (id != NULL && owner != NULL && label != NULL && command != NULL && visible) {
        FiremodCapability *item = firemod_capability_find(registry, id);
        if (item == NULL) {
            gint order = g_key_file_get_integer(key, "capability", "order", NULL);
            item = capability_new(id, owner, label,
                icon != NULL ? icon : "application-x-executable-symbolic",
                100 + MAX(0, order));
            item->external = TRUE;
            item->origin = FIREMOD_ORIGIN_EXTERNAL_MODULE;
            item->canonical_slot = -1;
            g_ptr_array_add(registry->items, item);
        } else {
            replace_text(&item->owner, owner);
            replace_text(&item->label, label);
            if (icon != NULL) replace_text(&item->icon_name, icon);
        }
        replace_text(&item->command, command);
        g_free(item->executable);
        item->executable = find_executable(executable);
        gboolean available = executable == NULL || item->executable != NULL;
        firemod_capability_set_state(item,
            available ? FIREMOD_ACTION_AVAILABLE : FIREMOD_ACTION_UNAVAILABLE,
            available ? "Registered dispatch contract available" :
                "Registered owner executable is unavailable");
    }
    g_free(id); g_free(owner); g_free(label); g_free(icon);
    g_free(command); g_free(executable); g_key_file_unref(key);
}

static void scan_directory(FiremodCapabilityRegistry *registry, const char *path)
{
    GDir *directory = path != NULL ? g_dir_open(path, 0, NULL) : NULL;
    if (directory == NULL) return;
    const char *name = NULL;
    while ((name = g_dir_read_name(directory)) != NULL) {
        if (!g_str_has_suffix(name, ".capability")) continue;
        char *file = g_build_filename(path, name, NULL);
        apply_manifest(registry, file);
        g_free(file);
    }
    g_dir_close(directory);
}

FiremodCapabilityRegistry *firemod_capability_registry_new(void)
{
    FiremodCapabilityRegistry *registry = g_new0(FiremodCapabilityRegistry, 1);
    registry->items = g_ptr_array_new_with_free_func(
        (GDestroyNotify)capability_free);
    register_builtins(registry);
    const char * const *system = g_get_system_data_dirs();
    guint count = 0;
    while (system[count] != NULL) count++;
    while (count > 0) {
        count--;
        char *directory = g_build_filename(
            system[count], "firemod", "capabilities.d", NULL);
        scan_directory(registry, directory);
        g_free(directory);
    }
    scan_directory(registry, FIREMOD_CAPABILITY_DIR);
    char *user = g_build_filename(g_get_user_data_dir(),
        "firemod", "capabilities.d", NULL);
    scan_directory(registry, user);
    g_free(user);
    scan_directory(registry, g_getenv("FIREMOD_CAPABILITY_DIR"));
    return registry;
}

void firemod_capability_registry_remove_user_actions(
    FiremodCapabilityRegistry *registry)
{
    if (registry == NULL) return;
    for (gint i = (gint)registry->items->len - 1; i >= 0; i--) {
        FiremodCapability *item = g_ptr_array_index(registry->items, (guint)i);
        if (item->origin == FIREMOD_ORIGIN_USER_APPLICATION ||
            item->origin == FIREMOD_ORIGIN_USER_SCRIPT) {
            g_ptr_array_remove_index(registry->items, (guint)i);
        }
    }
}

void firemod_capability_registry_add(
    FiremodCapabilityRegistry *registry,
    FiremodCapability *capability)
{
    g_return_if_fail(registry != NULL && capability != NULL);
    g_ptr_array_add(registry->items, capability);
}

FiremodCapability *firemod_capability_new_user(
    const gchar *id,
    const gchar *label,
    const gchar *short_label,
    const gchar *summary,
    const gchar *icon_path,
    const gchar *color,
    gchar **argv,
    const gchar *working_directory,
    gboolean run_in_terminal,
    gint order,
    FiremodActionOrigin origin)
{
    FiremodCapability *item = capability_new(id, "User action", label,
        "application-x-executable-symbolic", 100 + MAX(0, order));
    item->origin = origin;
    item->external = TRUE;
    item->canonical_slot = -1;
    replace_text(&item->short_label, short_label);
    replace_text(&item->summary, summary);
    replace_text(&item->icon_resource,
        "/org/philabs/Firemod/icons/utility-menu/run-script.png");
    replace_text(&item->icon_path, icon_path);
    replace_text(&item->color, color != NULL ? color : "#891688");
    item->argv = g_strdupv(argv);
    item->working_directory = g_strdup(working_directory);
    item->run_in_terminal = run_in_terminal;
    item->executable = argv != NULL && argv[0] != NULL ? find_executable(argv[0]) : NULL;
    firemod_capability_set_state(item,
        item->executable != NULL ? FIREMOD_ACTION_AVAILABLE : FIREMOD_ACTION_UNAVAILABLE,
        item->executable != NULL ? "User action target available" :
            "User action target is unavailable");
    return item;
}

void firemod_capability_registry_free(FiremodCapabilityRegistry *registry)
{
    if (registry == NULL) return;
    g_ptr_array_free(registry->items, TRUE);
    g_free(registry);
}

const gchar *firemod_action_state_name(FiremodActionState state)
{
    switch (state) {
    case FIREMOD_ACTION_AVAILABLE: return "Available";
    case FIREMOD_ACTION_UNAVAILABLE: return "Unavailable";
    case FIREMOD_ACTION_HIDDEN: return "Hidden";
    default: return "Rejected";
    }
}
