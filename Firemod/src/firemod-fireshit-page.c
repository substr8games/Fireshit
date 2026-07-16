#include "firemod-fireshit-page.h"

#include "firemod-action-editor.h"
#include "firemod-hex-preview.h"

struct FiremodFireshitPage {
    FiremodApp *app;
    GtkWidget *root;
    GtkSwitch *menu_enabled;
    GtkSpinButton *menu_scale;
    GtkSwitch *hover_enabled;
    GtkSpinButton *hover_delay;
    GtkCheckButton *canonical[FIREMOD_CANONICAL_COUNT];
    GtkListBox *actions;
    GtkLabel *status;
    GtkWidget *preview;
    gint selected;
};

static GtkWidget *section(const gchar *title)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "fireshit-section");
    GtkWidget *label = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "section-title");
    gtk_box_append(GTK_BOX(box), label);
    return box;
}

static GtkWidget *control_row(const gchar *label, GtkWidget *control)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "control-row");
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_widget_add_css_class(text, "control-label");
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), text);
    gtk_box_append(GTK_BOX(row), control);
    return row;
}

static GtkWidget *metadata_value(const gchar *text)
{
    GtkWidget *value = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(value), 1.0f);
    gtk_widget_add_css_class(value, "code-value");
    return value;
}

static GtkWidget *page_header(void)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_add_css_class(box, "page-header");
    GtkWidget *eyebrow = gtk_label_new("FIRESHIT / SUITE CONTROL");
    gtk_label_set_xalign(GTK_LABEL(eyebrow), 0.0f);
    gtk_widget_add_css_class(eyebrow, "page-eyebrow");
    GtkWidget *heading = gtk_label_new("Fireshit Settings");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_add_css_class(heading, "page-heading");
    GtkWidget *copy = gtk_label_new(
        "Configure the global utility grid, capture routes, custom actions, "
        "and the applied Fireshit registry.");
    gtk_label_set_xalign(GTK_LABEL(copy), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_widget_add_css_class(copy, "page-copy");
    gtk_box_append(GTK_BOX(box), eyebrow);
    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}

static void show_error(FiremodFireshitPage *page, const gchar *title, GError *error)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title);
    gtk_alert_dialog_set_detail(dialog,
        error != NULL ? error->message : "Unknown Firemod settings error");
    gtk_alert_dialog_show(dialog, page->app->window);
    g_object_unref(dialog);
}

static void changed(FiremodFireshitPage *page)
{
    firemod_settings_mark_dirty(page->app->settings);
    gtk_label_set_text(page->status,
        "UNSAVED DRAFT · live menu remains on the last applied configuration");
    firemod_hex_preview_refresh(page->preview);
}

static void menu_enabled_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    (void)pspec;
    FiremodFireshitPage *page = data;
    page->app->settings->draft->menu_enabled = gtk_switch_get_active(GTK_SWITCH(object));
    changed(page);
}

static void scale_changed(GtkSpinButton *spin, gpointer data)
{
    FiremodFireshitPage *page = data;
    page->app->settings->draft->menu_scale = gtk_spin_button_get_value(spin);
    changed(page);
}

static void hover_enabled_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    (void)pspec;
    FiremodFireshitPage *page = data;
    page->app->settings->draft->hover_enabled = gtk_switch_get_active(GTK_SWITCH(object));
    changed(page);
}

static void delay_changed(GtkSpinButton *spin, gpointer data)
{
    FiremodFireshitPage *page = data;
    page->app->settings->draft->hover_delay_ms =
        (guint)gtk_spin_button_get_value_as_int(spin);
    changed(page);
}

static void canonical_changed(GtkCheckButton *button, gpointer data)
{
    FiremodFireshitPage *page = data;
    guint slot = GPOINTER_TO_UINT(g_object_get_data(
        G_OBJECT(button), "firemod-slot"));
    page->app->settings->draft->canonical_visible[slot] =
        gtk_check_button_get_active(button);
    changed(page);
}

static void clear_list(GtkListBox *list)
{
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(list));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(list, child);
        child = next;
    }
}

static GtkWidget *action_row(FiremodCustomAction *action)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(copy, TRUE);
    GtkWidget *title = gtk_label_new(action->label);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "action-title");
    gchar *detail = g_strdup_printf("%s · %s · %s",
        action->type == FIREMOD_CUSTOM_SCRIPT ? "SCRIPT" : "APPLICATION",
        action->color, action->target);
    GtkWidget *subtitle = gtk_label_new(detail);
    g_free(detail);
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(subtitle), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_add_css_class(subtitle, "action-subtitle");
    GtkWidget *state = gtk_label_new(action->enabled ? "ENABLED" : "DISABLED");
    gtk_widget_add_css_class(state, "badge");
    gtk_widget_add_css_class(state, action->enabled ? "status-available" : "status-located");
    gtk_box_append(GTK_BOX(copy), title); gtk_box_append(GTK_BOX(copy), subtitle);
    gtk_box_append(GTK_BOX(row), copy); gtk_box_append(GTK_BOX(row), state);
    return row;
}

static void render_actions(FiremodFireshitPage *page)
{
    clear_list(page->actions);
    FiremodSettingsState *draft = page->app->settings->draft;
    for (guint i = 0; i < draft->actions->len; i++) {
        GtkWidget *row = action_row(g_ptr_array_index(draft->actions, i));
        g_object_set_data(G_OBJECT(row), "firemod-index", GUINT_TO_POINTER(i + 1));
        gtk_list_box_append(page->actions, row);
    }
    page->selected = -1;
}

static void selected_changed(GtkListBox *list, gpointer data)
{
    FiremodFireshitPage *page = data;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(list);
    page->selected = row != NULL ? gtk_list_box_row_get_index(row) : -1;
}

typedef struct {
    FiremodFireshitPage *page;
    gint index;
} EditResult;

static void edited(FiremodCustomAction *action, gpointer data)
{
    EditResult *result = data;
    FiremodSettingsState *draft = result->page->app->settings->draft;
    if (result->index >= 0 && (guint)result->index < draft->actions->len) {
        firemod_custom_action_free(g_ptr_array_index(draft->actions, result->index));
        g_ptr_array_index(draft->actions, result->index) = action;
    } else {
        g_ptr_array_add(draft->actions, action);
    }
    changed(result->page);
    render_actions(result->page);
    g_free(result);
}

static void open_editor(
    FiremodFireshitPage *page,
    const FiremodCustomAction *source,
    FiremodCustomType type,
    gint index)
{
    EditResult *result = g_new0(EditResult, 1);
    result->page = page; result->index = index;
    firemod_action_editor_show(page->app->window, source, type, edited, result);
}

static void add_application(GtkButton *button, gpointer data)
{
    (void)button;
    open_editor(data, NULL, FIREMOD_CUSTOM_APPLICATION, -1);
}

static void add_script(GtkButton *button, gpointer data)
{
    (void)button;
    open_editor(data, NULL, FIREMOD_CUSTOM_SCRIPT, -1);
}

static void edit_action(GtkButton *button, gpointer data)
{
    (void)button;
    FiremodFireshitPage *page = data;
    if (page->selected < 0) return;
    FiremodCustomAction *action = g_ptr_array_index(
        page->app->settings->draft->actions, (guint)page->selected);
    open_editor(page, action, action->type, page->selected);
}

static void duplicate_action(GtkButton *button, gpointer data)
{
    (void)button;
    FiremodFireshitPage *page = data;
    if (page->selected < 0) return;
    FiremodCustomAction *source = g_ptr_array_index(
        page->app->settings->draft->actions, (guint)page->selected);
    FiremodCustomAction *copy = firemod_custom_action_clone(source);
    g_free(copy->id);
    gchar *uuid = g_uuid_string_random();
    copy->id = g_strdup_printf("user.%s", uuid);
    g_free(uuid);
    gchar *label = g_strdup_printf("%s Copy", copy->label);
    g_free(copy->label); copy->label = label;
    g_ptr_array_add(page->app->settings->draft->actions, copy);
    changed(page); render_actions(page);
}

typedef struct {
    FiremodFireshitPage *page;
    gint index;
} RemoveRequest;

static void remove_chosen(GObject *source, GAsyncResult *result, gpointer data)
{
    RemoveRequest *request = data;
    GError *error = NULL;
    gint choice = gtk_alert_dialog_choose_finish(
        GTK_ALERT_DIALOG(source), result, &error);
    if (error == NULL && choice == 1) {
        GPtrArray *actions = request->page->app->settings->draft->actions;
        if (request->index >= 0 && (guint)request->index < actions->len) {
            g_ptr_array_remove_index(actions, (guint)request->index);
            changed(request->page);
            render_actions(request->page);
        }
    }
    g_clear_error(&error);
    g_free(request);
}

static void remove_action(GtkButton *button, gpointer data)
{
    (void)button;
    FiremodFireshitPage *page = data;
    if (page->selected < 0) return;
    FiremodCustomAction *action = g_ptr_array_index(
        page->app->settings->draft->actions, (guint)page->selected);
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Remove custom action?");
    gchar *detail = g_strdup_printf(
        "%s will be removed from the unsaved draft.", action->label);
    gtk_alert_dialog_set_detail(dialog, detail);
    g_free(detail);
    const gchar *buttons[] = {"Cancel", "Remove", NULL};
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);
    RemoveRequest *request = g_new0(RemoveRequest, 1);
    request->page = page;
    request->index = page->selected;
    gtk_alert_dialog_choose(dialog, page->app->window, NULL,
        remove_chosen, request);
    g_object_unref(dialog);
}

static void move_action(GtkButton *button, gpointer data)
{
    FiremodFireshitPage *page = data;
    if (page->selected < 0) return;
    gint delta = GPOINTER_TO_INT(g_object_get_data(
        G_OBJECT(button), "firemod-delta"));
    gint target = page->selected + delta;
    GPtrArray *actions = page->app->settings->draft->actions;
    if (target < 0 || target >= (gint)actions->len) return;
    gpointer item = g_ptr_array_index(actions, (guint)page->selected);
    g_ptr_array_index(actions, (guint)page->selected) =
        g_ptr_array_index(actions, (guint)target);
    g_ptr_array_index(actions, (guint)target) = item;
    changed(page); render_actions(page);
    GtkListBoxRow *row = gtk_list_box_get_row_at_index(page->actions, target);
    gtk_list_box_select_row(page->actions, row);
}

static void save_settings(GtkButton *button, gpointer data)
{
    (void)button;
    FiremodFireshitPage *page = data;
    GError *error = NULL;
    if (!firemod_settings_save(page->app->settings, &error)) {
        show_error(page, "Could not save Fireshit settings", error);
        g_clear_error(&error); return;
    }
    gtk_label_set_text(page->status, "SAVED · Apply to activate this revision");
}

static void apply_settings(GtkButton *button, gpointer data)
{
    (void)button;
    FiremodFireshitPage *page = data;
    GError *error = NULL;
    if (!firemod_settings_apply(page->app->settings,
            page->app->capabilities, &error)) {
        show_error(page, "Could not apply Fireshit settings", error);
        g_clear_error(&error); return;
    }
    gtk_label_set_text(page->status, "APPLIED · live registry updated");
    firemod_app_render(page->app);
}

static void discard_settings(GtkButton *button, gpointer data)
{
    (void)button;
    FiremodFireshitPage *page = data;
    firemod_settings_discard(page->app->settings);
    firemod_fireshit_page_refresh(page);
    gtk_label_set_text(page->status, "DRAFT DISCARDED · saved configuration restored");
}

static GtkWidget *button_row(FiremodFireshitPage *page)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(row, "action-toolbar");
    GtkWidget *add_app = gtk_button_new_with_label("Add Application");
    GtkWidget *add_script_button = gtk_button_new_with_label("Add Script");
    GtkWidget *edit = gtk_button_new_with_label("Edit");
    GtkWidget *duplicate = gtk_button_new_with_label("Duplicate");
    GtkWidget *remove = gtk_button_new_with_label("Remove");
    GtkWidget *earlier = gtk_button_new_with_label("Earlier");
    GtkWidget *later = gtk_button_new_with_label("Later");
    gtk_widget_add_css_class(remove, "destructive-action");
    g_object_set_data(G_OBJECT(earlier), "firemod-delta", GINT_TO_POINTER(-1));
    g_object_set_data(G_OBJECT(later), "firemod-delta", GINT_TO_POINTER(1));
    g_signal_connect(add_app, "clicked", G_CALLBACK(add_application), page);
    g_signal_connect(add_script_button, "clicked", G_CALLBACK(add_script), page);
    g_signal_connect(edit, "clicked", G_CALLBACK(edit_action), page);
    g_signal_connect(duplicate, "clicked", G_CALLBACK(duplicate_action), page);
    g_signal_connect(remove, "clicked", G_CALLBACK(remove_action), page);
    g_signal_connect(earlier, "clicked", G_CALLBACK(move_action), page);
    g_signal_connect(later, "clicked", G_CALLBACK(move_action), page);
    gtk_box_append(GTK_BOX(row), add_app); gtk_box_append(GTK_BOX(row), add_script_button);
    gtk_box_append(GTK_BOX(row), edit); gtk_box_append(GTK_BOX(row), duplicate);
    gtk_box_append(GTK_BOX(row), remove); gtk_box_append(GTK_BOX(row), earlier);
    gtk_box_append(GTK_BOX(row), later);
    return row;
}

FiremodFireshitPage *firemod_fireshit_page_new(FiremodApp *app)
{
    FiremodFireshitPage *page = g_new0(FiremodFireshitPage, 1);
    page->app = app; page->selected = -1;
    page->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(page->root, "page-canvas");
    gtk_box_append(GTK_BOX(page->root), page_header());
    GtkWidget *core = section("FIRESHIT CORE");
    gtk_box_append(GTK_BOX(core), control_row("Registry",
        metadata_value("Firemod-owned / capability projected")));
    gtk_box_append(GTK_BOX(core), control_row("Global binding",
        metadata_value("SUPER + MIDDLE CLICK")));
    gtk_box_append(GTK_BOX(page->root), core);
    GtkWidget *menu = section("GLOBAL UTILITY MENU");
    page->menu_enabled = GTK_SWITCH(gtk_switch_new());
    page->menu_scale = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0.5, 1.0, 0.05));
    page->hover_enabled = GTK_SWITCH(gtk_switch_new());
    page->hover_delay = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 2000, 25));
    gtk_box_append(GTK_BOX(menu), control_row("Enabled", GTK_WIDGET(page->menu_enabled)));
    gtk_box_append(GTK_BOX(menu), control_row("Base Scale", GTK_WIDGET(page->menu_scale)));
    gtk_box_append(GTK_BOX(menu), control_row("Hover Text", GTK_WIDGET(page->hover_enabled)));
    gtk_box_append(GTK_BOX(menu), control_row("Hover Delay (ms)", GTK_WIDGET(page->hover_delay)));
    GtkWidget *canonical = gtk_flow_box_new();
    gtk_widget_add_css_class(canonical, "canonical-grid");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(canonical), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(canonical), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(canonical), 4);
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        page->canonical[i] = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
            firemod_settings_canonical_label(i)));
        g_object_set_data(G_OBJECT(page->canonical[i]), "firemod-slot", GUINT_TO_POINTER(i));
        g_signal_connect(page->canonical[i], "toggled", G_CALLBACK(canonical_changed), page);
        gtk_flow_box_append(GTK_FLOW_BOX(canonical), GTK_WIDGET(page->canonical[i]));
    }
    gtk_box_append(GTK_BOX(menu), canonical);
    gtk_box_append(GTK_BOX(page->root), menu);
    GtkWidget *custom = section("CUSTOM ACTIONS");
    gtk_box_append(GTK_BOX(custom), button_row(page));
    page->actions = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(page->actions), "custom-action-list");
    g_signal_connect(page->actions, "row-selected", G_CALLBACK(selected_changed), page);
    gtk_box_append(GTK_BOX(custom), GTK_WIDGET(page->actions));
    gtk_box_append(GTK_BOX(page->root), custom);
    GtkWidget *preview_section = section("LAYOUT PREVIEW");
    page->preview = firemod_hex_preview_new(app->capabilities, app->settings);
    gtk_box_append(GTK_BOX(preview_section), page->preview);
    gtk_box_append(GTK_BOX(page->root), preview_section);
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(footer, "settings-footer");
    page->status = GTK_LABEL(gtk_label_new("Saved configuration loaded"));
    gtk_label_set_xalign(page->status, 0.0f); gtk_widget_set_hexpand(GTK_WIDGET(page->status), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(page->status), "settings-status");
    GtkWidget *discard = gtk_button_new_with_label("Discard");
    GtkWidget *save = gtk_button_new_with_label("Save");
    GtkWidget *apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(discard, "destructive-action");
    gtk_widget_add_css_class(apply, "primary-action");
    g_signal_connect(discard, "clicked", G_CALLBACK(discard_settings), page);
    g_signal_connect(save, "clicked", G_CALLBACK(save_settings), page);
    g_signal_connect(apply, "clicked", G_CALLBACK(apply_settings), page);
    gtk_box_append(GTK_BOX(footer), GTK_WIDGET(page->status));
    gtk_box_append(GTK_BOX(footer), discard); gtk_box_append(GTK_BOX(footer), save);
    gtk_box_append(GTK_BOX(footer), apply);
    gtk_box_append(GTK_BOX(page->root), footer);
    g_signal_connect(page->menu_enabled, "notify::active", G_CALLBACK(menu_enabled_changed), page);
    g_signal_connect(page->menu_scale, "value-changed", G_CALLBACK(scale_changed), page);
    g_signal_connect(page->hover_enabled, "notify::active", G_CALLBACK(hover_enabled_changed), page);
    g_signal_connect(page->hover_delay, "value-changed", G_CALLBACK(delay_changed), page);
    firemod_fireshit_page_refresh(page);
    return page;
}

GtkWidget *firemod_fireshit_page_widget(FiremodFireshitPage *page)
{
    return page != NULL ? page->root : NULL;
}

void firemod_fireshit_page_refresh(FiremodFireshitPage *page)
{
    FiremodSettingsState *draft = page->app->settings->draft;
    g_signal_handlers_block_by_func(page->menu_enabled, menu_enabled_changed, page);
    g_signal_handlers_block_by_func(page->menu_scale, scale_changed, page);
    g_signal_handlers_block_by_func(page->hover_enabled, hover_enabled_changed, page);
    g_signal_handlers_block_by_func(page->hover_delay, delay_changed, page);
    gtk_switch_set_active(page->menu_enabled, draft->menu_enabled);
    gtk_spin_button_set_value(page->menu_scale, draft->menu_scale);
    gtk_switch_set_active(page->hover_enabled, draft->hover_enabled);
    gtk_spin_button_set_value(page->hover_delay, draft->hover_delay_ms);
    g_signal_handlers_unblock_by_func(page->menu_enabled, menu_enabled_changed, page);
    g_signal_handlers_unblock_by_func(page->menu_scale, scale_changed, page);
    g_signal_handlers_unblock_by_func(page->hover_enabled, hover_enabled_changed, page);
    g_signal_handlers_unblock_by_func(page->hover_delay, delay_changed, page);
    for (guint i = 0; i < FIREMOD_CANONICAL_COUNT; i++) {
        g_signal_handlers_block_by_func(page->canonical[i], canonical_changed, page);
        gtk_check_button_set_active(page->canonical[i], draft->canonical_visible[i]);
        g_signal_handlers_unblock_by_func(page->canonical[i], canonical_changed, page);
    }
    render_actions(page); firemod_hex_preview_refresh(page->preview);
}

void firemod_fireshit_page_free(FiremodFireshitPage *page)
{
    g_free(page);
}
