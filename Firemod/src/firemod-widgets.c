#include "firemod-widgets.h"

#include "firemod-authority-widget.h"
#include "firemod-actions.h"

static void run_settings_clicked(GtkButton *button, gpointer user_data)
{
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(button));
    firemod_action_run(
        GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : NULL,
        (const char *)user_data);
}

GtkWidget *firemod_widget_badge(const char *text, const char *css_class)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_add_css_class(label, "badge");
    if (css_class != NULL) gtk_widget_add_css_class(label, css_class);
    return label;
}

gboolean firemod_module_matches(
    FiremodModule *module,
    FiremodFilter filter,
    const char *query)
{
    if (filter == FIREMOD_FILTER_FIRESHIT && !module->first_party) return FALSE;
    if (filter == FIREMOD_FILTER_READABLE) {
        gboolean readable = FALSE;
        for (guint i = 0; i < module->authorities->len; i++) {
            FiremodAuthority *authority = g_ptr_array_index(module->authorities, i);
            readable |= authority->capability >= FIREMOD_CAPABILITY_READABLE;
        }
        if (!readable) return FALSE;
    }
    if (filter == FIREMOD_FILTER_LOCATED) {
        gboolean located = FALSE;
        for (guint i = 0; i < module->authorities->len; i++) {
            FiremodAuthority *authority = g_ptr_array_index(module->authorities, i);
            located |= authority->capability == FIREMOD_CAPABILITY_LOCATED;
        }
        if (!located) return FALSE;
    }
    if (firemod_text_matches(module->name, query) ||
        firemod_text_matches(module->summary, query)) {
        return TRUE;
    }
    for (guint i = 0; i < module->authorities->len; i++) {
        if (firemod_authority_matches(
                g_ptr_array_index(module->authorities, i), query)) {
            return TRUE;
        }
    }
    return query == NULL || *query == '\0';
}

GtkWidget *firemod_widget_module_card(
    FiremodApp *app,
    FiremodModule *module,
    gboolean settings_only,
    const char *query)
{
    (void)app;
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "module-card");
    if (module->first_party) gtk_widget_add_css_class(card, "first-party");
    if (module->uncertain) gtk_widget_add_css_class(card, "uncertain");
    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name(module->icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_widget_add_css_class(icon, "module-icon");
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(copy, TRUE);
    GtkWidget *name = gtk_label_new(module->name);
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_widget_add_css_class(name, "module-name");
    GtkWidget *summary = gtk_label_new(module->summary);
    gtk_label_set_xalign(GTK_LABEL(summary), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_widget_add_css_class(summary, "module-summary");
    gtk_box_append(GTK_BOX(copy), name);
    gtk_box_append(GTK_BOX(copy), summary);
    gtk_box_append(GTK_BOX(head), icon);
    gtk_box_append(GTK_BOX(head), copy);
    if (module->available && module->settings_command != NULL) {
        GtkWidget *settings = gtk_button_new_with_label("Native Settings");
        gtk_widget_add_css_class(settings, "compact-button");
        g_signal_connect(settings, "clicked", G_CALLBACK(run_settings_clicked),
            module->settings_command);
        gtk_box_append(GTK_BOX(head), settings);
    }
    gtk_box_append(GTK_BOX(head), firemod_widget_badge(
        module->available ? firemod_confidence_name(module->confidence) : "Unavailable",
        module->available ? (module->first_party ? "registered" : "recognized") : "unavailable"));
    gtk_box_append(GTK_BOX(card), head);
    if (!settings_only && module->availability_note != NULL) {
        GtkWidget *status = gtk_label_new(module->availability_note);
        gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(status), PANGO_ELLIPSIZE_MIDDLE);
        gtk_widget_add_css_class(status, "module-status");
        gtk_box_append(GTK_BOX(card), status);
    }
    for (guint i = 0; i < module->authorities->len; i++) {
        FiremodAuthority *authority = g_ptr_array_index(module->authorities, i);
        if (query != NULL && *query != '\0' &&
            !firemod_authority_matches(authority, query) &&
            !firemod_text_matches(module->name, query) &&
            !firemod_text_matches(module->summary, query)) {
            continue;
        }
        gtk_box_append(GTK_BOX(card), firemod_authority_widget_new(authority));
    }
    return card;
}

GtkWidget *firemod_widget_empty(const char *title, const char *detail)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    GtkWidget *icon = gtk_image_new_from_icon_name("system-search-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_widget_add_css_class(icon, "empty-icon");
    GtkWidget *heading = gtk_label_new(title);
    gtk_widget_add_css_class(heading, "empty-title");
    GtkWidget *copy = gtk_label_new(detail);
    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_label_set_justify(GTK_LABEL(copy), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(copy, "empty-detail");
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}
