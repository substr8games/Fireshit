#include "firemod-authority-widget.h"

#include <string.h>

#include "firemod-actions.h"
#include "firemod-widgets.h"

static GtkWindow *button_parent(GtkWidget *widget)
{
    GtkRoot *root = gtk_widget_get_root(widget);
    return GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL;
}

static void free_signal_data(gpointer data, GClosure *closure)
{
    (void)closure;
    g_free(data);
}

static void open_path_clicked(GtkButton *button, gpointer user_data)
{
    firemod_action_open_path(
        button_parent(GTK_WIDGET(button)), (const char *)user_data);
}

static void open_folder_clicked(GtkButton *button, gpointer user_data)
{
    firemod_action_open_folder(
        button_parent(GTK_WIDGET(button)), (const char *)user_data);
}

static void copy_path_clicked(GtkButton *button, gpointer user_data)
{
    firemod_action_copy_path(GTK_WIDGET(button), (const char *)user_data);
    gtk_button_set_label(button, "Copied");
}

static GtkWidget *action_button(
    const char *label,
    GCallback callback,
    const char *path)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(button, "compact-button");
    g_signal_connect_data(
        button, "clicked", callback, g_strdup(path), free_signal_data, 0);
    return button;
}

static GtkWidget *setting_row(FiremodSetting *setting)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(row, "setting-row");
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(text, TRUE);
    GtkWidget *name = gtk_label_new(setting->name);
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_widget_add_css_class(name, "setting-name");
    GtkWidget *path = gtk_label_new(setting->key_path);
    gtk_label_set_xalign(GTK_LABEL(path), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(path), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(path, "setting-path");
    gtk_box_append(GTK_BOX(text), name);
    gtk_box_append(GTK_BOX(text), path);
    GtkWidget *value = gtk_label_new(setting->value);
    gtk_label_set_selectable(GTK_LABEL(value), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(value), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(value), 32);
    gtk_widget_add_css_class(value, "setting-value");
    gtk_box_append(GTK_BOX(row), text);
    gtk_box_append(GTK_BOX(row), value);
    return row;
}

GtkWidget *firemod_authority_widget_new(FiremodAuthority *authority)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "authority-card");
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(head, "authority-head");
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(copy, TRUE);
    GtkWidget *name = gtk_label_new(authority->name);
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_widget_add_css_class(name, "authority-name");
    GtkWidget *type = gtk_label_new(firemod_authority_type_name(authority->type));
    gtk_label_set_xalign(GTK_LABEL(type), 0.0f);
    gtk_widget_add_css_class(type, "authority-type");
    gtk_box_append(GTK_BOX(copy), name);
    gtk_box_append(GTK_BOX(copy), type);
    gtk_box_append(GTK_BOX(head), copy);
    gtk_box_append(GTK_BOX(head), firemod_widget_badge(
        firemod_capability_name(authority->capability),
        authority->capability == FIREMOD_CAPABILITY_READABLE ? "readable" : "located"));
    gtk_box_append(GTK_BOX(card), head);

    if (authority->path != NULL) {
        GtkWidget *path = gtk_label_new(authority->path);
        gtk_label_set_xalign(GTK_LABEL(path), 0.0f);
        gtk_label_set_selectable(GTK_LABEL(path), TRUE);
        gtk_label_set_ellipsize(GTK_LABEL(path), PANGO_ELLIPSIZE_MIDDLE);
        gtk_widget_add_css_class(path, "source-path");
        gtk_box_append(GTK_BOX(card), path);
    }
    if (authority->detail != NULL) {
        GtkWidget *detail = gtk_label_new(authority->detail);
        gtk_label_set_xalign(GTK_LABEL(detail), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(detail), TRUE);
        gtk_widget_add_css_class(detail, "authority-detail");
        gtk_box_append(GTK_BOX(card), detail);
    }
    for (guint i = 0; i < authority->settings->len; i++) {
        gtk_box_append(GTK_BOX(card), setting_row(
            g_ptr_array_index(authority->settings, i)));
    }
    if (authority->path != NULL && g_file_test(authority->path, G_FILE_TEST_EXISTS)) {
        GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_add_css_class(actions, "authority-actions");
        gtk_box_append(GTK_BOX(actions), action_button(
            g_file_test(authority->path, G_FILE_TEST_IS_DIR) ? "Open" : "Open File",
            G_CALLBACK(open_path_clicked), authority->path));
        gtk_box_append(GTK_BOX(actions), action_button(
            "Open Folder", G_CALLBACK(open_folder_clicked), authority->path));
        gtk_box_append(GTK_BOX(actions), action_button(
            "Copy Path", G_CALLBACK(copy_path_clicked), authority->path));
        gtk_box_append(GTK_BOX(card), actions);
    }
    return card;
}

gboolean firemod_text_matches(const char *text, const char *query)
{
    if (query == NULL || *query == '\0') return TRUE;
    if (text == NULL) return FALSE;
    char *haystack = g_utf8_strdown(text, -1);
    gboolean match = strstr(haystack, query) != NULL;
    g_free(haystack);
    return match;
}

gboolean firemod_authority_matches(FiremodAuthority *authority, const char *query)
{
    if (firemod_text_matches(authority->name, query) ||
        firemod_text_matches(authority->path, query)) {
        return TRUE;
    }
    for (guint i = 0; i < authority->settings->len; i++) {
        FiremodSetting *setting = g_ptr_array_index(authority->settings, i);
        if (firemod_text_matches(setting->name, query) ||
            firemod_text_matches(setting->value, query)) {
            return TRUE;
        }
    }
    return FALSE;
}
