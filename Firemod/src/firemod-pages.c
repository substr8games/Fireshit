#include "firemod-pages.h"
#include "firemod-widgets.h"
static void clear_box(GtkWidget *box)
{
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}
static GtkWidget *section_title(
    const char *eyebrow,
    const char *title,
    const char *detail)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_add_css_class(box, "page-header");
    GtkWidget *kicker = gtk_label_new(eyebrow);
    gtk_label_set_xalign(GTK_LABEL(kicker), 0.0f);
    gtk_widget_add_css_class(kicker, "page-eyebrow");
    GtkWidget *heading = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_add_css_class(heading, "page-heading");
    GtkWidget *copy = gtk_label_new(detail);
    gtk_label_set_xalign(GTK_LABEL(copy), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_widget_add_css_class(copy, "page-copy");
    gtk_box_append(GTK_BOX(box), kicker);
    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}
static GtkWidget *metric_card(const char *value, const char *label, const char *css)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(card, TRUE);
    gtk_widget_add_css_class(card, "metric-card");
    if (css != NULL) gtk_widget_add_css_class(card, css);
    GtkWidget *number = gtk_label_new(value);
    gtk_label_set_xalign(GTK_LABEL(number), 0.0f);
    gtk_widget_add_css_class(number, "metric-value");
    GtkWidget *caption = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(caption), 0.0f);
    gtk_widget_add_css_class(caption, "metric-label");
    gtk_box_append(GTK_BOX(card), number);
    gtk_box_append(GTK_BOX(card), caption);
    return card;
}
static void render_overview(FiremodApp *app)
{
    clear_box(app->overview_page);
    GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_add_css_class(hero, "hero-card");
    GtkWidget *picture = gtk_picture_new_for_resource(
        "/org/philabs/Firemod/firemod-machine.png");
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request(picture, 190, 140);
    gtk_widget_add_css_class(picture, "hero-art");
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand(copy, TRUE);
    gtk_widget_set_valign(copy, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(copy, "hero-copy-block");
    GtkWidget *kicker = gtk_label_new("FIRESHIT CORE / WAYFIRE MODULE SHELL");
    gtk_label_set_xalign(GTK_LABEL(kicker), 0.0f);
    gtk_widget_add_css_class(kicker, "hero-kicker");
    GtkWidget *title = gtk_label_new("Map everything. Touch nothing by accident.");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(title), TRUE);
    gtk_widget_add_css_class(title, "hero-title");
    GtkWidget *body = gtk_label_new(
        "Firemod has assembled a curated inventory of settings authorities. "
        "Editing remains locked until each adapter earns write authority.");
    gtk_label_set_xalign(GTK_LABEL(body), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(body), TRUE);
    gtk_widget_add_css_class(body, "hero-copy");
    gtk_box_append(GTK_BOX(copy), kicker);
    gtk_box_append(GTK_BOX(copy), title);
    gtk_box_append(GTK_BOX(copy), body);
    gtk_box_append(GTK_BOX(hero), picture);
    gtk_box_append(GTK_BOX(hero), copy);
    gtk_box_append(GTK_BOX(app->overview_page), hero);
    guint readable = 0;
    guint authorities = 0;
    for (guint i = 0; i < app->inventory->modules->len; i++) {
        FiremodModule *module = g_ptr_array_index(app->inventory->modules, i);
        authorities += module->authorities->len;
        for (guint j = 0; j < module->authorities->len; j++) {
            FiremodAuthority *authority = g_ptr_array_index(module->authorities, j);
            if (authority->capability >= FIREMOD_CAPABILITY_READABLE) readable++;
        }
    }
    char modules_text[32];
    char authority_text[32];
    char readable_text[32];
    g_snprintf(modules_text, sizeof modules_text, "%u", app->inventory->modules->len);
    g_snprintf(authority_text, sizeof authority_text, "%u", authorities);
    g_snprintf(readable_text, sizeof readable_text, "%u", readable);
    GtkWidget *metrics = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(metrics, "metrics-strip");
    gtk_box_append(GTK_BOX(metrics), metric_card(modules_text, "Cleanly grouped modules", "orange"));
    gtk_box_append(GTK_BOX(metrics), metric_card(authority_text, "Located authorities", "cyan"));
    gtk_box_append(GTK_BOX(metrics), metric_card(readable_text, "Readable authorities", "green"));
    gtk_box_append(GTK_BOX(app->overview_page), metrics);
    GtkWidget *law = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(law, "law-card");
    GtkWidget *law_title = gtk_label_new("DISCOVERY IS BROAD. MUTATION IS EARNED.");
    gtk_label_set_xalign(GTK_LABEL(law_title), 0.0f);
    gtk_widget_add_css_class(law_title, "law-title");
    GtkWidget *law_copy = gtk_label_new(
        "Inspect values, trace ownership, open the native authority, and reveal the "
        "archaeological layer only when you deliberately ask for it.");
    gtk_label_set_xalign(GTK_LABEL(law_copy), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(law_copy), TRUE);
    gtk_box_append(GTK_BOX(law), law_title);
    gtk_box_append(GTK_BOX(law), law_copy);
    gtk_box_append(GTK_BOX(app->overview_page), law);
}
static guint render_module_list(
    FiremodApp *app,
    GtkWidget *target,
    GPtrArray *modules,
    gboolean settings_only,
    const char *query)
{
    guint shown = 0;
    for (guint i = 0; i < modules->len; i++) {
        FiremodModule *module = g_ptr_array_index(modules, i);
        if (!firemod_module_matches(module, app->active_filter, query)) continue;
        if (settings_only && module->authorities->len == 0) continue;
        gtk_box_append(GTK_BOX(target), firemod_widget_module_card(
            app, module, settings_only, query));
        shown++;
    }
    return shown;
}
static char *normalized_query(FiremodApp *app)
{
    const char *text = gtk_editable_get_text(GTK_EDITABLE(app->search));
    return g_utf8_strdown(text != NULL ? text : "", -1);
}
static void render_inventory(FiremodApp *app)
{
    clear_box(app->inventory_page);
    gtk_box_append(GTK_BOX(app->inventory_page), section_title(
        "FIREMOD / AUTHORITY MAP",
        "Settings inventory",
        "Only cleanly attributed applications and authorities appear here by default."));
    char *query = normalized_query(app);
    guint shown = render_module_list(
        app, app->inventory_page, app->inventory->modules, TRUE, query);
    if (shown == 0) {
        gtk_box_append(GTK_BOX(app->inventory_page), firemod_widget_empty(
            "No matching settings",
            "Try a broader search or another capability filter."));
    }
    g_free(query);
}
static void render_modules(FiremodApp *app)
{
    clear_box(app->modules_page);
    gtk_box_append(GTK_BOX(app->modules_page), section_title(
        "FIRESHIT / CAPABILITY REGISTRY",
        "Registered modules",
        "Fireshit modules stay independent; Firemod exposes only the contracts they support."));
    char *query = normalized_query(app);
    guint shown = render_module_list(
        app, app->modules_page, app->inventory->modules, FALSE, query);
    if (shown == 0) {
        gtk_box_append(GTK_BOX(app->modules_page), firemod_widget_empty(
            "No matching modules", "The registry has no module matching this filter."));
    }
    g_free(query);
}
static void render_diagnostics(FiremodApp *app)
{
    clear_box(app->diagnostics_page);
    gtk_box_append(GTK_BOX(app->diagnostics_page), section_title(
        "FIREMOD / DIAGNOSTIC LAYER",
        "Ungrouped and uncertain results",
        "This is filesystem archaeology, intentionally separated from the curated inventory."));
    if (!app->show_uncertain) {
        gtk_box_append(GTK_BOX(app->diagnostics_page), firemod_widget_empty(
            "Diagnostics are hidden",
            "Enable “Show ungrouped and uncertain results” in the sidebar to inspect them."));
        return;
    }
    char *query = normalized_query(app);
    guint shown = render_module_list(
        app, app->diagnostics_page, app->inventory->uncertain_modules, FALSE, query);
    if (shown == 0) {
        gtk_box_append(GTK_BOX(app->diagnostics_page), firemod_widget_empty(
            "No uncertain matches", "Nothing in the diagnostic layer matches this search."));
    }
    g_free(query);
}
void firemod_pages_render(FiremodApp *app)
{
    render_overview(app);
    render_inventory(app);
    render_modules(app);
    render_diagnostics(app);
}
