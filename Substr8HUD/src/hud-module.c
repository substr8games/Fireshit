#include "hud-module.h"

#include <glib/gstdio.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>

#include "hud-app.h"
#include "hud-dnd.h"
#include "hud-settings.h"

static gboolean program_available(const char *name)
{
    if (name == NULL || *name == '\0') {
        return FALSE;
    }

    if (g_path_is_absolute(name) || strchr(name, '/') != NULL) {
        return g_file_test(name, G_FILE_TEST_IS_EXECUTABLE);
    }

    gchar *path = g_find_program_in_path(name);
    gboolean found = path != NULL;
    g_free(path);
    return found;
}

static gchar *missing_requirements(HudModule *module)
{
    GString *missing = g_string_new(NULL);

    for (guint index = 0;
         module->requires != NULL && module->requires[index] != NULL;
         index++) {
        if (program_available(module->requires[index])) {
            continue;
        }

        if (missing->len > 0) {
            g_string_append(missing, ", ");
        }
        g_string_append(missing, module->requires[index]);
    }

    return g_string_free(missing, FALSE);
}

const char *hud_module_slot_name(HudModuleSlot slot)
{
    static const char *names[] = {
        "top-left",
        "top-center",
        "top-right",
        "bottom-left-wide",
        "bottom-right-wide",
    };

    if (slot < 0 || slot >= HUD_SLOT_COUNT) {
        return "top-left";
    }
    return names[slot];
}

gboolean hud_module_slot_parse(
    const char *text,
    HudModuleSlot *slot)
{
    if (text == NULL || slot == NULL) {
        return FALSE;
    }

    for (int value = 0; value < HUD_SLOT_COUNT; value++) {
        if (g_strcmp0(text, hud_module_slot_name((HudModuleSlot)value)) == 0) {
            *slot = (HudModuleSlot)value;
            return TRUE;
        }
    }

    if (g_strcmp0(text, "bottom-wide") == 0 ||
        g_strcmp0(text, "bottom-left") == 0) {
        *slot = HUD_SLOT_BOTTOM_LEFT_WIDE;
        return TRUE;
    }
    if (g_strcmp0(text, "bottom-right") == 0) {
        *slot = HUD_SLOT_BOTTOM_RIGHT_WIDE;
        return TRUE;
    }

    return FALSE;
}

static void set_status(
    HudModule *module,
    HudModuleStatus status)
{
    module->status = status;

    if (module->status_label != NULL) {
        gtk_label_set_text(
            GTK_LABEL(module->status_label),
            hud_module_status_name(status));
    }
}

static void feed_notice(
    HudModule *module,
    const char *format,
    ...)
{
    if (module->terminal == NULL) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    gchar *message = g_strdup_vprintf(format, arguments);
    va_end(arguments);

    vte_terminal_feed(module->terminal, message, -1);
    g_free(message);
}

static gboolean restart_module(gpointer user_data)
{
    HudModule *module = user_data;
    module->restart_source = 0;
    hud_module_spawn(module);
    return G_SOURCE_REMOVE;
}

static gboolean force_stop_timeout(gpointer user_data)
{
    HudModule *module = user_data;
    module->kill_source = 0;
    hud_module_force_stop(module);
    return G_SOURCE_REMOVE;
}

static void on_focus_changed(
    GObject *object,
    GParamSpec *parameter,
    gpointer user_data)
{
    (void)parameter;
    HudModule *module = user_data;
    if (gtk_widget_has_focus(GTK_WIDGET(object))) {
        module->app->last_focused_slot = module->slot;
        module->app->has_last_focused_slot = TRUE;
    }
}

static void on_child_exited(
    VteTerminal *terminal,
    gint status,
    gpointer user_data)
{
    (void)terminal;
    HudModule *module = user_data;
    gboolean intentional = module->stopping;
    gboolean restart = !intentional &&
        g_strcmp0(module->restart_policy, "always") == 0;
    module->pid = 0;
    module->stopping = FALSE;

    if (module->kill_source != 0) {
        g_source_remove(module->kill_source);
        module->kill_source = 0;
    }

    if (intentional || status == 0) {
        set_status(module, HUD_MODULE_STOPPED);
        feed_notice(module, "\r\n[module stopped]\r\n");
    } else {
        set_status(module, HUD_MODULE_FAILED);
        feed_notice(
            module,
            "\r\n[module exited with status %d]\r\n",
            status);
    }

    hud_app_log(
        module->app,
        "module %s exited with status %d",
        module->id,
        status);

    if (restart && module->restart_source == 0) {
        feed_notice(module, "[module restarting]\r\n");
        module->restart_source = g_timeout_add_seconds(
            1,
            restart_module,
            module);
    }
}

static void on_spawned(
    VteTerminal *terminal,
    GPid pid,
    GError *error,
    gpointer user_data)
{
    HudModule *module = user_data;

    if (error != NULL) {
        set_status(module, HUD_MODULE_FAILED);
        gchar *message = g_strdup_printf(
            "\r\n[spawn failed] %s\r\n",
            error->message);
        vte_terminal_feed(terminal, message, -1);
        hud_app_log(
            module->app,
            "module %s spawn failed: %s",
            module->id,
            error->message);
        g_free(message);
        return;
    }

    module->pid = pid;
    module->stopping = FALSE;
    set_status(module, HUD_MODULE_RUNNING);
    vte_terminal_set_input_enabled(
        terminal,
        module->app->module_input_enabled &&
        module->interactive &&
        module->pane != NULL &&
        gtk_widget_get_mapped(module->pane));
    hud_app_log(
        module->app,
        "module %s spawned with pid %d",
        module->id,
        (int)pid);
}

const char *hud_module_status_name(HudModuleStatus status)
{
    static const char *names[] = {
        "READY",
        "RUNNING",
        "STOPPED",
        "UNAVAILABLE",
        "FAILED",
    };

    if (status < 0 || status > HUD_MODULE_FAILED) {
        return "UNKNOWN";
    }
    return names[status];
}

HudModule *hud_module_load(
    HudApp *app,
    const char *path,
    GError **error)
{
    GKeyFile *key = g_key_file_new();

    if (!g_key_file_load_from_file(
            key,
            path,
            G_KEY_FILE_NONE,
            error)) {
        g_key_file_unref(key);
        return NULL;
    }

    HudModule *module = g_new0(HudModule, 1);
    module->app = app;
    module->source_path = g_strdup(path);
    module->id = g_key_file_get_string(key, "module", "id", error);

    if (module->id == NULL) {
        hud_module_free(module);
        g_key_file_unref(key);
        return NULL;
    }

    module->title = g_key_file_get_string(
        key, "module", "title", error);
    module->command = g_key_file_get_string(
        key, "module", "command", error);
    gchar *slot = g_key_file_get_string(
        key, "module", "layout_slot", error);

    if (module->title == NULL ||
        module->command == NULL ||
        slot == NULL) {
        g_free(slot);
        hud_module_free(module);
        g_key_file_unref(key);
        return NULL;
    }

    if (!hud_module_slot_parse(slot, &module->slot)) {
        g_set_error(
            error,
            G_KEY_FILE_ERROR,
            G_KEY_FILE_ERROR_INVALID_VALUE,
            "invalid layout_slot '%s' in %s",
            slot,
            path);
        g_free(slot);
        hud_module_free(module);
        g_key_file_unref(key);
        return NULL;
    }
    g_free(slot);

    module->default_slot = module->slot;

    module->working_directory = g_key_file_get_string(
        key, "module", "working_directory", NULL);
    module->lifecycle = g_key_file_get_string(
        key, "module", "lifecycle", NULL);
    module->restart_policy = g_key_file_get_string(
        key, "module", "restart_policy", NULL);
    module->bank_order = g_key_file_has_key(
        key, "module", "bank_order", NULL)
        ? g_key_file_get_integer(key, "module", "bank_order", NULL)
        : 100;
    module->default_bank_order = module->bank_order;
    hud_settings_apply_module(app, module);

    module->interactive = g_key_file_get_boolean(
        key, "module", "interactive", NULL);
    module->scrollback_lines = g_key_file_get_integer(
        key, "module", "scrollback_lines", NULL);
    module->requires = g_key_file_get_string_list(
        key, "module", "requires", NULL, NULL);

    if (module->working_directory == NULL ||
        *module->working_directory == '\0') {
        g_free(module->working_directory);
        module->working_directory = g_strdup(g_get_home_dir());
    }
    if (module->lifecycle == NULL) {
        module->lifecycle = g_strdup("visible");
    }
    if (module->restart_policy == NULL) {
        module->restart_policy = g_strdup("never");
    }
    if (module->scrollback_lines <= 0) {
        module->scrollback_lines = 10000;
    }

    gchar *missing = missing_requirements(module);
    module->status = *missing == '\0'
        ? HUD_MODULE_READY
        : HUD_MODULE_UNAVAILABLE;

    if (*missing != '\0') {
        hud_app_log(
            app,
            "module %s unavailable; missing: %s",
            module->id,
            missing);
        g_object_set_data_full(
            G_OBJECT(app->application),
            module->id,
            missing,
            g_free);
    } else {
        g_free(missing);
    }

    g_key_file_unref(key);
    return module;
}

void hud_module_free(HudModule *module)
{
    if (module == NULL) {
        return;
    }

    if (module->restart_source != 0) {
        g_source_remove(module->restart_source);
    }
    if (module->kill_source != 0) {
        g_source_remove(module->kill_source);
    }

    g_free(module->source_path);
    g_free(module->id);
    g_free(module->title);
    g_free(module->command);
    g_free(module->working_directory);
    g_free(module->lifecycle);
    g_free(module->restart_policy);
    g_strfreev(module->requires);
    g_free(module);
}

GtkWidget *hud_module_create_pane(HudModule *module)
{
    GtkWidget *pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *selection_mark = gtk_label_new("◆");
    GtkWidget *drag_handle = gtk_label_new("⠿");
    GtkWidget *title = gtk_label_new(module->title);
    GtkWidget *status = gtk_label_new(
        hud_module_status_name(module->status));
    VteTerminal *terminal = VTE_TERMINAL(vte_terminal_new());

    module->pane = pane;
    module->status_label = status;
    module->terminal = terminal;

    gtk_widget_add_css_class(pane, "hud-pane");
    gtk_widget_add_css_class(header, "hud-pane-header");
    gtk_widget_add_css_class(selection_mark, "hud-selection-mark");
    gtk_widget_add_css_class(drag_handle, "hud-drag-handle");
    gtk_widget_add_css_class(title, "hud-pane-title");
    gtk_widget_add_css_class(status, "hud-pane-status");
    gtk_widget_add_css_class(
        GTK_WIDGET(terminal),
        "hud-terminal");

    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(status), 1.0f);
    gtk_widget_set_hexpand(GTK_WIDGET(terminal), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(terminal), TRUE);

    GdkRGBA foreground = {0.85, 0.88, 0.90, 1.0};
    GdkRGBA background = {0.01, 0.025, 0.04, 1.0};
    vte_terminal_set_color_foreground(terminal, &foreground);
    vte_terminal_set_color_background(terminal, &background);
    vte_terminal_set_clear_background(terminal, FALSE);
    vte_terminal_set_scrollback_lines(
        terminal,
        module->scrollback_lines);
    vte_terminal_set_scroll_on_output(terminal, TRUE);
    vte_terminal_set_scroll_on_keystroke(terminal, TRUE);
    vte_terminal_set_mouse_autohide(terminal, TRUE);
    vte_terminal_set_cursor_blink_mode(
        terminal,
        VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_input_enabled(terminal, FALSE);

    PangoFontDescription *font =
        pango_font_description_from_string("monospace 10");
    vte_terminal_set_font(terminal, font);
    pango_font_description_free(font);

    g_signal_connect(
        terminal,
        "child-exited",
        G_CALLBACK(on_child_exited),
        module);
    g_signal_connect(
        terminal,
        "notify::has-focus",
        G_CALLBACK(on_focus_changed),
        module);

    gtk_box_append(GTK_BOX(header), selection_mark);
    gtk_box_append(GTK_BOX(header), drag_handle);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), status);
    gtk_box_append(GTK_BOX(pane), header);
    gtk_box_append(GTK_BOX(pane), GTK_WIDGET(terminal));
    hud_dnd_attach_module(module, header);

    if (module->status == HUD_MODULE_UNAVAILABLE) {
        const char *missing = g_object_get_data(
            G_OBJECT(module->app->application),
            module->id);
        feed_notice(
            module,
            "MODULE UNAVAILABLE\r\nmissing command(s): %s\r\n",
            missing != NULL ? missing : "unknown");
    }

    return pane;
}

void hud_module_spawn(HudModule *module)
{
    if (module == NULL ||
        module->terminal == NULL ||
        module->pid > 0 ||
        module->status == HUD_MODULE_RUNNING ||
        module->status == HUD_MODULE_UNAVAILABLE ||
        g_strcmp0(module->lifecycle, "manual") == 0) {
        if (module != NULL &&
            g_strcmp0(module->lifecycle, "manual") == 0) {
            set_status(module, HUD_MODULE_STOPPED);
            feed_notice(module, "MODULE STOPPED\r\nmanual lifecycle\r\n");
        }
        return;
    }

    if (module->status == HUD_MODULE_STOPPED ||
        module->status == HUD_MODULE_FAILED) {
        vte_terminal_reset(module->terminal, TRUE, TRUE);
        set_status(module, HUD_MODULE_READY);
    }

    module->stopping = FALSE;
    if (module->restart_source != 0) {
        g_source_remove(module->restart_source);
        module->restart_source = 0;
    }

    const char *argv[] = {
        "/usr/bin/bash",
        "--noprofile",
        "--norc",
        "-lc",
        module->command,
        NULL,
    };

    vte_terminal_spawn_with_fds_async(
        module->terminal,
        VTE_PTY_DEFAULT,
        module->working_directory,
        argv,
        NULL,
        NULL,
        0,
        NULL,
        0,
        G_SPAWN_SEARCH_PATH,
        NULL,
        NULL,
        NULL,
        -1,
        NULL,
        on_spawned,
        module);
}


void hud_module_stop(HudModule *module)
{
    if (module == NULL) {
        return;
    }

    if (module->restart_source != 0) {
        g_source_remove(module->restart_source);
        module->restart_source = 0;
    }
    if (module->pid <= 0) {
        return;
    }

    GPid pid = module->pid;
    module->stopping = TRUE;
    set_status(module, HUD_MODULE_STOPPED);
    hud_module_set_input(module, FALSE);

    if (kill(pid, SIGTERM) != 0) {
        hud_app_log(
            module->app,
            "module %s could not terminate pid %d",
            module->id,
            (int)pid);
        return;
    }

    if (module->kill_source != 0) {
        g_source_remove(module->kill_source);
    }
    module->kill_source = g_timeout_add(
        1500,
        force_stop_timeout,
        module);

    hud_app_log(
        module->app,
        "module %s terminating pid %d",
        module->id,
        (int)pid);
}

void hud_module_force_stop(HudModule *module)
{
    if (module == NULL || module->pid <= 0) {
        return;
    }

    GPid pid = module->pid;
    if (kill(pid, SIGKILL) == 0) {
        hud_app_log(
            module->app,
            "module %s force-killed pid %d",
            module->id,
            (int)pid);
    }
}

gboolean hud_module_is_running(HudModule *module)
{
    return module != NULL && module->pid > 0;
}

void hud_module_set_input(
    HudModule *module,
    gboolean enabled)
{
    if (module == NULL || module->terminal == NULL) {
        return;
    }

    gboolean admitted = enabled &&
        module->interactive &&
        module->status == HUD_MODULE_RUNNING;
    vte_terminal_set_input_enabled(
        module->terminal,
        admitted);
}

gboolean hud_module_focus(HudModule *module)
{
    if (module == NULL ||
        module->terminal == NULL ||
        !module->interactive ||
        module->status != HUD_MODULE_RUNNING) {
        return FALSE;
    }

    return gtk_widget_grab_focus(
        GTK_WIDGET(module->terminal));
}
