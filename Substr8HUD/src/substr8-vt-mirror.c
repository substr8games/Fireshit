#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
static volatile sig_atomic_t running = 1;
static void stop_running(int signal_number)
{
    (void)signal_number;
    running = 0;
}
static int parse_vt(const char *text)
{
    if (text == NULL) return 0;
    while (*text == ' ' || *text == '\t') text++;
    if (strncmp(text, "tty", 3) == 0) text += 3;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    return end != text && *end == '\0' && value > 0 && value < 64 ? (int)value : 0;
}
static int configured_vt(void)
{
    const char *config_home = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char path[4096];
    if (config_home != NULL && *config_home != '\0')
        snprintf(path, sizeof(path), "%s/substr8-hud/tty.conf", config_home);
    else if (home != NULL && *home != '\0')
        snprintf(path, sizeof(path), "%s/.config/substr8-hud/tty.conf", home);
    else return 0;
    FILE *file = fopen(path, "r");
    if (file == NULL) return 0;
    char line[128];
    int value = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "VT=", 3) == 0) {
            value = parse_vt(line + 3);
            break;
        }
    }
    fclose(file);
    return value;
}
static int detected_vt(void)
{
    int value = parse_vt(getenv("SUBSTR8_HUD_TTY"));
    if (value == 0) value = configured_vt();
    if (value == 0) value = parse_vt(getenv("XDG_VTNR"));
    if (value != 0) return value;
    char active[32] = {0};
    FILE *file = fopen("/sys/class/tty/tty0/active", "r");
    if (file != NULL) {
        if (fgets(active, sizeof(active), file) != NULL) value = parse_vt(active);
        fclose(file);
    }
    return value != 0 ? value : 1;
}
static void output_size(unsigned *rows, unsigned *cols)
{
    struct winsize window = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 &&
        window.ws_row != 0 && window.ws_col != 0) {
        *rows = window.ws_row;
        *cols = window.ws_col;
    }
}
static void set_color(unsigned char attribute)
{
    static const unsigned char rgb[16][3] = {
        {0,0,0},{170,0,0},{0,170,0},{170,85,0},
        {0,0,170},{170,0,170},{0,170,170},{170,170,170},
        {85,85,85},{255,85,85},{85,255,85},{255,255,85},
        {85,85,255},{255,85,255},{85,255,255},{255,255,255},
    };
    unsigned foreground = attribute & 0x0f;
    unsigned background = (attribute >> 4) & 0x07;
    printf("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um",
        rgb[foreground][0], rgb[foreground][1], rgb[foreground][2],
        rgb[background][0], rgb[background][1], rgb[background][2]);
}
static void show_unavailable(int vt, const char *device, const char *reason)
{
    printf("\033[2J\033[H\033[0mTTY%d — READ ONLY\n\n", vt);
    printf("MIRROR UNAVAILABLE\n%s\n\n", reason);
    printf("SOURCE %s\nCHECK  ./run.sh module-check\n", device);
    printf("\033[?25l");
    fflush(stdout);
}
static void render(const unsigned char header[4], const uint16_t *cells,
    unsigned output_rows, unsigned output_cols)
{
    unsigned source_rows = header[0], source_cols = header[1];
    unsigned shown_rows = output_rows < source_rows ? output_rows : source_rows;
    unsigned shown_cols = output_cols < source_cols ? output_cols : source_cols;
    unsigned first_row = source_rows - shown_rows;
    unsigned char previous_attribute = 0xff;
    for (unsigned row = 0; row < shown_rows; row++) {
        printf("\033[%u;1H", row + 1);
        for (unsigned col = 0; col < shown_cols; col++) {
            uint16_t cell = cells[(first_row + row) * source_cols + col];
            unsigned char character = cell & 0xff;
            unsigned char attribute = (cell >> 8) & 0xff;
            if (attribute != previous_attribute) {
                set_color(attribute);
                previous_attribute = attribute;
            }
            putchar(character >= 32 && character < 127 ? character : ' ');
        }
        printf("\033[0m\033[K");
        previous_attribute = 0xff;
    }
    for (unsigned row = shown_rows; row < output_rows; row++)
        printf("\033[%u;1H\033[0m\033[K", row + 1);
    unsigned cursor_row = header[3], cursor_col = header[2];
    if (cursor_row >= first_row && cursor_row < source_rows && cursor_col < shown_cols)
        printf("\033[%u;%uH\033[?25h", cursor_row - first_row + 1, cursor_col + 1);
    else printf("\033[?25l");
    fflush(stdout);
}
static void wait_retry(void)
{
    struct timespec delay = {.tv_sec = 2, .tv_nsec = 0};
    while (running && nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
}
int main(int argc, char **argv)
{
    int vt = detected_vt();
    if (argc == 2) {
        vt = parse_vt(argv[1]);
        if (vt == 0) {
            fprintf(stderr, "usage: substr8-vt-mirror [VT_NUMBER|ttyN]\n");
            return 2;
        }
    }
    signal(SIGINT, stop_running); signal(SIGTERM, stop_running); signal(SIGHUP, stop_running);
    printf("\033[2J\033[H\033[?7l\033[?25l");
    char device[32];
    snprintf(device, sizeof(device), "/dev/vcsa%d", vt);
    uint16_t *cells = NULL;
    size_t allocation = 0;
    struct timespec refresh = {.tv_sec = 0, .tv_nsec = 150000000};
    while (running) {
        int descriptor = open(device, O_RDONLY | O_CLOEXEC);
        if (descriptor < 0) {
            show_unavailable(vt, device, strerror(errno));
            wait_retry();
            continue;
        }
        unsigned char header[4] = {0};
        unsigned prior_rows = 0, prior_cols = 0;
        uint64_t prior_hash = 0;
        while (running) {
            if (pread(descriptor, header, 4, 0) != 4 || header[0] == 0 || header[1] == 0)
                break;
            size_t bytes = (size_t)header[0] * header[1] * sizeof(uint16_t);
            if (bytes > allocation) {
                uint16_t *replacement = realloc(cells, bytes);
                if (replacement == NULL) {
                    close(descriptor);
                    free(cells);
                    return 1;
                }
                cells = replacement;
                allocation = bytes;
            }
            if (pread(descriptor, cells, bytes, 4) != (ssize_t)bytes) break;
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned index = 0; index < 4; index++)
                hash = (hash ^ header[index]) * 1099511628211ULL;
            const unsigned char *raw = (const unsigned char *)cells;
            for (size_t index = 0; index < bytes; index++)
                hash = (hash ^ raw[index]) * 1099511628211ULL;
            unsigned rows = header[0], cols = header[1];
            output_size(&rows, &cols);
            if (hash != prior_hash || rows != prior_rows || cols != prior_cols) {
                render(header, cells, rows, cols);
                prior_hash = hash; prior_rows = rows; prior_cols = cols;
            }
            nanosleep(&refresh, NULL);
        }
        close(descriptor);
        if (running) {
            show_unavailable(vt, device, "virtual-console buffer read failed");
            wait_retry();
        }
    }
    printf("\033[0m\033[?7h\033[?25h\n");
    free(cells);
    return 0;
}
