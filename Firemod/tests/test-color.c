#include <glib.h>

#include "firemod-color.h"

static void test_black(void)
{
    FiremodColor *color = firemod_color_new(0, 0, 0);
    g_assert_cmpstr(color->hex, ==, "#000000");
    g_assert_cmpstr(color->rgb, ==, "rgb(0, 0, 0)");
    g_assert_cmpstr(color->hsl, ==, "hsl(0.0 0.0% 0.0%)");
    g_assert_true(g_str_has_prefix(color->oklch, "oklch(0.00%"));
    firemod_color_free(color);
}

static void test_primary_red(void)
{
    FiremodColor *color = firemod_color_new(255, 0, 0);
    g_assert_cmpstr(color->hex, ==, "#FF0000");
    g_assert_cmpstr(color->hsl, ==, "hsl(0.0 100.0% 50.0%)");
    g_assert_true(g_str_has_prefix(color->oklch, "oklch(62.80% 0.2577"));
    firemod_color_free(color);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/color/black", test_black);
    g_test_add_func("/color/red", test_primary_red);
    return g_test_run();
}
