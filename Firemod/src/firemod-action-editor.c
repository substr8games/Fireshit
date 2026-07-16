#include "firemod-action-editor.h"

#include <math.h>

typedef struct {
    GtkWindow *window;
    FiremodCustomAction *action;
    GtkEntry *name;
    GtkEntry *short_label;
    GtkEntry *hover_title;
    GtkEntry *hover_subtitle;
    GtkEntry *target;
    GtkTextView *arguments;
    GtkEntry *working_directory;
    GtkEntry *icon;
    GtkColorDialogButton *color;
    GtkCheckButton *terminal;
    GtkCheckButton *enabled;
    FiremodActionEditorDone done;
    gpointer user_data;
} ActionEditor;

static GtkWidget *field_row(GtkGrid *grid, gint row, const gchar *label, GtkWidget *field)
{
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0.0f);
    gtk_widget_add_css_class(text, "field-label");
    gtk_grid_attach(grid, text, 0, row, 1, 1);
    gtk_widget_set_hexpand(field, TRUE);
    gtk_grid_attach(grid, field, 1, row, 1, 1);
    return field;
}

static void replace(gchar **target, const gchar *value)
{
    g_free(*target);
    *target = g_strdup(value != NULL ? value : "");
}

static gchar **arguments_from_view(GtkTextView *view)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    gchar **lines = g_strsplit(text, "\n", -1);
    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; lines[i] != NULL; i++) {
        gchar *trimmed = g_strstrip(lines[i]);
        if (*trimmed != '\0') g_ptr_array_add(items, g_strdup(trimmed));
    }
    g_ptr_array_add(items, NULL);
    g_strfreev(lines);
    g_free(text);
    return (gchar **)g_ptr_array_free(items, FALSE);
}

static gchar *rgba_hex(const GdkRGBA *color)
{
    return g_strdup_printf("#%02X%02X%02X",
        (guint)round(CLAMP(color->red, 0.0, 1.0) * 255.0),
        (guint)round(CLAMP(color->green, 0.0, 1.0) * 255.0),
        (guint)round(CLAMP(color->blue, 0.0, 1.0) * 255.0));
}

static void show_error(ActionEditor *editor, const gchar *detail)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("Custom action is not ready");
    gtk_alert_dialog_set_detail(dialog, detail);
    gtk_alert_dialog_show(dialog, editor->window);
    g_object_unref(dialog);
}

static void save_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    ActionEditor *editor = user_data;
    replace(&editor->action->label, gtk_editable_get_text(GTK_EDITABLE(editor->name)));
    replace(&editor->action->short_label,
        gtk_editable_get_text(GTK_EDITABLE(editor->short_label)));
    replace(&editor->action->hover_title,
        gtk_editable_get_text(GTK_EDITABLE(editor->hover_title)));
    replace(&editor->action->hover_subtitle,
        gtk_editable_get_text(GTK_EDITABLE(editor->hover_subtitle)));
    replace(&editor->action->target,
        gtk_editable_get_text(GTK_EDITABLE(editor->target)));
    replace(&editor->action->working_directory,
        gtk_editable_get_text(GTK_EDITABLE(editor->working_directory)));
    replace(&editor->action->icon_path,
        gtk_editable_get_text(GTK_EDITABLE(editor->icon)));
    g_strfreev(editor->action->arguments);
    editor->action->arguments = arguments_from_view(editor->arguments);
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(editor->color);
    gchar *color = rgba_hex(rgba);
    replace(&editor->action->color, color);
    g_free(color);
    editor->action->run_in_terminal = gtk_check_button_get_active(editor->terminal);
    editor->action->enabled = gtk_check_button_get_active(editor->enabled);
    FiremodSettingsState test = {0};
    test.menu_scale = 1.0; test.hover_delay_ms = 200;
    test.actions = g_ptr_array_new();
    g_ptr_array_add(test.actions, editor->action);
    gchar *detail = NULL;
    gboolean valid = firemod_settings_validate(&test, &detail);
    g_ptr_array_free(test.actions, TRUE);
    if (!valid) {
        show_error(editor, detail);
        g_free(detail);
        return;
    }
    FiremodCustomAction *result = editor->action;
    editor->action = NULL;
    editor->done(result, editor->user_data);
    gtk_window_destroy(editor->window);
}

static void editor_free(gpointer data)
{
    ActionEditor *editor = data;
    firemod_custom_action_free(editor->action);
    g_free(editor);
}

static void file_selected(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkEntry *entry = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    if (file != NULL) {
        gchar *path = g_file_get_path(file);
        gtk_editable_set_text(GTK_EDITABLE(entry), path != NULL ? path : "");
        g_free(path);
        g_object_unref(file);
    }
    g_clear_error(&error);
    g_object_unref(entry);
}

static void browse_clicked(GtkButton *button, gpointer user_data)
{
    GtkEntry *entry = user_data;
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_open(dialog, parent, NULL, file_selected,
        g_object_ref(entry));
    g_object_unref(dialog);
}

static GtkWidget *path_field(GtkEntry **entry_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *entry = gtk_entry_new();
    GtkWidget *browse = gtk_button_new_with_label("Browse");
    gtk_widget_add_css_class(browse, "compact-button");
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), browse);
    g_signal_connect(browse, "clicked", G_CALLBACK(browse_clicked), entry);
    *entry_out = GTK_ENTRY(entry);
    return box;
}


static GtkWidget *editor_header(
    gboolean editing,
    FiremodCustomType type)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_add_css_class(box, "editor-header");
    GtkWidget *eyebrow = gtk_label_new("FIRESHIT / CUSTOM ACTION");
    gtk_label_set_xalign(GTK_LABEL(eyebrow), 0.0f);
    gtk_widget_add_css_class(eyebrow, "page-eyebrow");
    const gchar *title_text = editing ? "Edit Menu Action" :
        (type == FIREMOD_CUSTOM_SCRIPT ? "Add Script Action" :
            "Add Application Action");
    GtkWidget *title = gtk_label_new(title_text);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "page-heading");
    GtkWidget *copy = gtk_label_new(
        "Define the launch contract and presentation. Accept updates the "
        "current draft; Save and Apply remain separate.");
    gtk_label_set_xalign(GTK_LABEL(copy), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_widget_add_css_class(copy, "page-copy");
    gtk_box_append(GTK_BOX(box), eyebrow);
    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}

void firemod_action_editor_show(
    GtkWindow *parent,
    const FiremodCustomAction *source,
    FiremodCustomType type,
    FiremodActionEditorDone done,
    gpointer user_data)
{
    ActionEditor *editor = g_new0(ActionEditor, 1);
    editor->action = source != NULL ? firemod_custom_action_clone(source) :
        firemod_custom_action_new(type);
    editor->done = done; editor->user_data = user_data;
    editor->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(editor->window, source != NULL ?
        "Edit Menu Action" : "Add Menu Action");
    gtk_window_set_transient_for(editor->window, parent);
    gtk_window_set_modal(editor->window, TRUE);
    gtk_window_set_default_size(editor->window, 680, 640);
    g_object_set_data_full(G_OBJECT(editor->window), "firemod-editor",
        editor, editor_free);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(root, "editor-root");
    gtk_box_append(GTK_BOX(root), editor_header(source != NULL, type));
    GtkWidget *grid_widget = gtk_grid_new();
    gtk_widget_add_css_class(grid_widget, "editor-grid");
    GtkGrid *grid = GTK_GRID(grid_widget);
    gtk_grid_set_row_spacing(grid, 10); gtk_grid_set_column_spacing(grid, 14);
    editor->name = GTK_ENTRY(gtk_entry_new());
    editor->short_label = GTK_ENTRY(gtk_entry_new());
    editor->hover_title = GTK_ENTRY(gtk_entry_new());
    editor->hover_subtitle = GTK_ENTRY(gtk_entry_new());
    field_row(grid, 0, "Name", GTK_WIDGET(editor->name));
    field_row(grid, 1, "Short Label", GTK_WIDGET(editor->short_label));
    field_row(grid, 2, "Hover Title", GTK_WIDGET(editor->hover_title));
    field_row(grid, 3, "Hover Subtitle", GTK_WIDGET(editor->hover_subtitle));
    GtkWidget *target = path_field(&editor->target);
    field_row(grid, 4, "Target", target);
    editor->arguments = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_monospace(editor->arguments, TRUE);
    gtk_text_view_set_wrap_mode(editor->arguments, GTK_WRAP_NONE);
    GtkWidget *arg_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(arg_scroll, -1, 110);
    gtk_widget_add_css_class(arg_scroll, "code-block");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(arg_scroll),
        GTK_WIDGET(editor->arguments));
    field_row(grid, 5, "Arguments\n(one per line)", arg_scroll);
    editor->working_directory = GTK_ENTRY(gtk_entry_new());
    field_row(grid, 6, "Working Directory", GTK_WIDGET(editor->working_directory));
    GtkWidget *icon = path_field(&editor->icon);
    field_row(grid, 7, "Custom PNG", icon);
    GtkColorDialog *color_dialog = gtk_color_dialog_new();
    editor->color = GTK_COLOR_DIALOG_BUTTON(
        gtk_color_dialog_button_new(color_dialog));
    field_row(grid, 8, "Color", GTK_WIDGET(editor->color));
    editor->terminal = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Run in terminal"));
    editor->enabled = GTK_CHECK_BUTTON(gtk_check_button_new_with_label("Enabled"));
    field_row(grid, 9, "Execution", GTK_WIDGET(editor->terminal));
    field_row(grid, 10, "Menu", GTK_WIDGET(editor->enabled));
    gtk_box_append(GTK_BOX(root), grid_widget);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    gtk_widget_add_css_class(actions, "editor-actions");
    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    GtkWidget *save = gtk_button_new_with_label("Accept");
    gtk_widget_add_css_class(save, "primary-action");
    g_signal_connect_swapped(cancel, "clicked",
        G_CALLBACK(gtk_window_destroy), editor->window);
    g_signal_connect(save, "clicked", G_CALLBACK(save_clicked), editor);
    gtk_box_append(GTK_BOX(actions), cancel); gtk_box_append(GTK_BOX(actions), save);
    gtk_box_append(GTK_BOX(root), actions);
    gtk_window_set_child(editor->window, root);
#define SET_ENTRY(widget, value) gtk_editable_set_text(GTK_EDITABLE(widget), value != NULL ? value : "")
    SET_ENTRY(editor->name, editor->action->label);
    SET_ENTRY(editor->short_label, editor->action->short_label);
    SET_ENTRY(editor->hover_title, editor->action->hover_title);
    SET_ENTRY(editor->hover_subtitle, editor->action->hover_subtitle);
    SET_ENTRY(editor->target, editor->action->target);
    SET_ENTRY(editor->working_directory, editor->action->working_directory);
    SET_ENTRY(editor->icon, editor->action->icon_path);
#undef SET_ENTRY
    gchar *args = g_strjoinv("\n", editor->action->arguments);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(editor->arguments), args, -1);
    g_free(args);
    GdkRGBA initial;
    if (!gdk_rgba_parse(&initial, editor->action->color)) gdk_rgba_parse(&initial, "#891688");
    gtk_color_dialog_button_set_rgba(editor->color, &initial);
    gtk_check_button_set_active(editor->terminal, editor->action->run_in_terminal);
    gtk_check_button_set_active(editor->enabled, editor->action->enabled);
    gtk_window_present(editor->window);
}
