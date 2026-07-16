#include "firemod-hex-assets.h"

#include <gio/gio.h>
#include <string.h>

struct FiremodHexAssets {
    GHashTable *surfaces;
};

typedef struct {
    const guchar *data;
    gsize length;
    gsize offset;
} PngStream;

static cairo_status_t read_png(
    void *closure,
    unsigned char *data,
    unsigned int length)
{
    PngStream *stream = closure;
    if (stream->offset + length > stream->length) {
        return CAIRO_STATUS_READ_ERROR;
    }
    memcpy(data, stream->data + stream->offset, length);
    stream->offset += length;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *load_resource(const gchar *path)
{
    GError *error = NULL;
    GBytes *bytes = g_resources_lookup_data(
        path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
    if (bytes == NULL) {
        g_clear_error(&error);
        return NULL;
    }
    gsize length = 0;
    const guchar *data = g_bytes_get_data(bytes, &length);
    PngStream stream = {data, length, 0};
    cairo_surface_t *surface = cairo_image_surface_create_from_png_stream(
        read_png, &stream);
    g_bytes_unref(bytes);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return NULL;
    }
    return surface;
}

static const gchar *asset_key(const FiremodCapability *capability)
{
    if (capability->icon_path != NULL && *capability->icon_path != '\0') {
        return capability->icon_path;
    }
    if (capability->icon_resource != NULL && *capability->icon_resource != '\0') {
        return capability->icon_resource;
    }
    return "/org/philabs/Firemod/icons/utility-menu/run-script.png";
}

static cairo_surface_t *load_surface(const gchar *key)
{
    if (g_str_has_prefix(key, "/org/philabs/Firemod/")) {
        return load_resource(key);
    }
    if (!g_str_has_suffix(key, ".png") && !g_str_has_suffix(key, ".PNG")) {
        return NULL;
    }
    cairo_surface_t *surface = cairo_image_surface_create_from_png(key);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return NULL;
    }
    return surface;
}

static cairo_surface_t *get_surface(
    FiremodHexAssets *assets,
    const FiremodCapability *capability)
{
    const gchar *key = asset_key(capability);
    cairo_surface_t *surface = g_hash_table_lookup(assets->surfaces, key);
    if (surface != NULL) return surface;
    surface = load_surface(key);
    if (surface == NULL && !g_str_has_suffix(key, "run-script.png")) {
        key = "/org/philabs/Firemod/icons/utility-menu/run-script.png";
        surface = g_hash_table_lookup(assets->surfaces, key);
        if (surface == NULL) surface = load_surface(key);
    }
    if (surface != NULL) {
        g_hash_table_insert(assets->surfaces, g_strdup(key), surface);
    }
    return surface;
}

FiremodHexAssets *firemod_hex_assets_new(void)
{
    FiremodHexAssets *assets = g_new0(FiremodHexAssets, 1);
    assets->surfaces = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, (GDestroyNotify)cairo_surface_destroy);
    return assets;
}

void firemod_hex_assets_free(FiremodHexAssets *assets)
{
    if (assets == NULL) return;
    g_hash_table_unref(assets->surfaces);
    g_free(assets);
}

gboolean firemod_hex_assets_draw(
    FiremodHexAssets *assets,
    cairo_t *cr,
    const FiremodCapability *capability,
    gdouble center_x,
    gdouble center_y,
    gdouble maximum,
    gdouble opacity)
{
    cairo_surface_t *surface = get_surface(assets, capability);
    if (surface == NULL) return FALSE;
    gint width = cairo_image_surface_get_width(surface);
    gint height = cairo_image_surface_get_height(surface);
    if (width <= 0 || height <= 0) return FALSE;
    gdouble factor = MIN(1.0, MIN(maximum / width, maximum / height));
    gdouble draw_width = width * factor;
    gdouble draw_height = height * factor;
    cairo_save(cr);
    cairo_translate(cr, center_x - draw_width * 0.5,
        center_y - draw_height * 0.5);
    cairo_scale(cr, factor, factor);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint_with_alpha(cr, opacity);
    cairo_restore(cr);
    return TRUE;
}
