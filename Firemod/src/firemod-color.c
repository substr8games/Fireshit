#include "firemod-color.h"

#include <math.h>

static double srgb_linear(guint8 value)
{
    double channel = value / 255.0;
    return channel <= 0.04045 ? channel / 12.92 :
        pow((channel + 0.055) / 1.055, 2.4);
}

static gchar *format_hsl(guint8 red, guint8 green, guint8 blue)
{
    double r = red / 255.0;
    double g = green / 255.0;
    double b = blue / 255.0;
    double max = MAX(r, MAX(g, b));
    double min = MIN(r, MIN(g, b));
    double delta = max - min;
    double light = (max + min) / 2.0;
    double hue = 0.0;
    double saturation = 0.0;
    if (delta > 0.0) {
        saturation = delta / (1.0 - fabs(2.0 * light - 1.0));
        if (max == r) hue = 60.0 * fmod((g - b) / delta, 6.0);
        else if (max == g) hue = 60.0 * (((b - r) / delta) + 2.0);
        else hue = 60.0 * (((r - g) / delta) + 4.0);
        if (hue < 0.0) hue += 360.0;
    }
    return g_strdup_printf("hsl(%.1f %.1f%% %.1f%%)",
        hue, saturation * 100.0, light * 100.0);
}

static gchar *format_oklch(guint8 red, guint8 green, guint8 blue)
{
    double r = srgb_linear(red);
    double g = srgb_linear(green);
    double b = srgb_linear(blue);
    double l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
    double m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
    double s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;
    double lp = cbrt(l);
    double mp = cbrt(m);
    double sp = cbrt(s);
    double light = 0.2104542553 * lp + 0.7936177850 * mp - 0.0040720468 * sp;
    double axis_a = 1.9779984951 * lp - 2.4285922050 * mp + 0.4505937099 * sp;
    double axis_b = 0.0259040371 * lp + 0.7827717662 * mp - 0.8086757660 * sp;
    double chroma = hypot(axis_a, axis_b);
    double hue = atan2(axis_b, axis_a) * 180.0 / G_PI;
    if (hue < 0.0) hue += 360.0;
    if (chroma < 0.000001) hue = 0.0;
    return g_strdup_printf("oklch(%.2f%% %.4f %.2f)",
        light * 100.0, chroma, hue);
}

FiremodColor *firemod_color_new(guint8 red, guint8 green, guint8 blue)
{
    FiremodColor *color = g_new0(FiremodColor, 1);
    color->red = red;
    color->green = green;
    color->blue = blue;
    color->hex = g_strdup_printf("#%02X%02X%02X", red, green, blue);
    color->rgb = g_strdup_printf("rgb(%u, %u, %u)", red, green, blue);
    color->hsl = format_hsl(red, green, blue);
    color->oklch = format_oklch(red, green, blue);
    return color;
}

void firemod_color_free(FiremodColor *color)
{
    if (color == NULL) return;
    g_free(color->hex);
    g_free(color->rgb);
    g_free(color->hsl);
    g_free(color->oklch);
    g_free(color);
}
