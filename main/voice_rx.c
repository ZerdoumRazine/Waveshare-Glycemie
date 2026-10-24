/*
 * voice_rx.c: Serial transfer of WAV files to the microSD.
 * Design note: Verify-only operations must never write to the card.
 */

/*
 * Transfers pre-generated WAV files through a bounded serial protocol to microSD.
 * Transfer banks WAV par USB- series with integrity checks.
 */
#include "voice_rx.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sd_storage.h"
#include "voice_cmd.h"

static const char *TAG = "voix_rx";

#define CONSOLE_BAUD     115200
#define FILE_MAX         (256 * 1024)
#define CHUNK            896  /* < tampon de reception UART de la console (1024 octets) : aucun debordement possible */
#define CHUNK_TIMEOUT_MS 3000
#define IDLE_END_US      (30 * 1000000LL)

typedef struct {
    bool crc;
    int files;
    uint64_t bytes;
} list_ctx_t;

static const uart_port_t k_port = (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM;
static uint8_t *s_buf;   /* file received (PSRAM) */
static uint8_t *s_check; /* relecture sur la carte (PSRAM) */
static bool s_session;
/* Sub-folder of the bank in the process of copying: "" for the French (root of SD_VOICE_DIR),
 * sinon le code de la langue ("en", "es", "de", "it", "pt"). Remis a zero a chaque session. */
static char s_subdir[8];

static int64_t s_last_us;

/* Protocol response, sent immediately: PC waits for each acquittal before next block */
static void reply(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void reply(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

static uint32_t crc32(const uint8_t *data, size_t len)
{
    return esp_rom_crc32_le(0, data, (uint32_t)len); /* CRC-32 IEEE, identique a zlib.crc32 */
}

static bool valid_name(const char *name)
{
    const size_t len = strlen(name);
    if (len < 5 || len > 44 || strcmp(name + len - 4, ".wav") != 0) {
        return false;
    }
    for (size_t i = 0; i < len - 4; i++) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

static void set_baud(uint32_t baud)
{
    uint32_t current = 0;
    uart_get_baudrate(k_port, &current);
    if (current != baud) {
        fflush(stdout);
        uart_wait_tx_done(k_port, pdMS_TO_TICKS(500));
        vTaskDelay(pdMS_TO_TICKS(50));
        uart_set_baudrate(k_port, baud);
    }
}

/* Restore the console baud rate and release temporary transfer buffers. */
static void end_session(const char *why)
{
    s_session = false;
    set_baud(CONSOLE_BAUD);
    free(s_buf);
    free(s_check);
    s_buf = NULL;
    s_check = NULL;
    sd_storage_refresh_voice_files();
    ESP_LOGD(TAG, "Session de copie de la banque vocale terminee (%s) : %d fichier(s) WAV sur la microSD", why,
             sd_storage_voice_files());
}

/* Verify microSD readiness and negotiate a temporary faster serial baud rate. */
static void cmd_begin(const char *args)
{
    s_subdir[0] = '\0'; /* chaque session repart de la banque francaise (racine) */
    const long baud = args[0] != '\0' ? strtol(args, NULL, 10) : CONSOLE_BAUD;
    if (baud != 115200 && baud != 230400 && baud != 460800 && baud != 921600) {
        reply("VRX BEGIN ERR debit\n");
        return;
    }
    if (s_buf == NULL) {
        s_buf = heap_caps_malloc(FILE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_check == NULL) {
        s_check = heap_caps_malloc(FILE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_buf == NULL || s_check == NULL) {
        reply("VRX BEGIN ERR memoire\n");
        return;
    }
    uint32_t free_mb = 0;
    const int files = sd_storage_list_voice(NULL, NULL, NULL, &free_mb);
    if (files < 0) {
        reply("VRX BEGIN ERR microSD indisponible\n");
        return;
    }
    reply("VRX BEGIN OK baud=%ld fichiers=%d libre_mo=%" PRIu32 " crc_test=%08" PRIx32 "\n", baud, files, free_mb,
           crc32((const uint8_t *)"123456789", 9));
    s_session = true;
    s_last_us = esp_timer_get_time();
    set_baud((uint32_t)baud);
}

/* binary blocks acquit one by one in s_buf */
static bool receive(const char *name, size_t size)
{
    size_t got = 0;
    while (got < size) {
        const size_t want = size - got < CHUNK ? size - got : CHUNK;
        size_t have = 0;
        while (have < want) {
            const int n = uart_read_bytes(k_port, s_buf + got + have, (uint32_t)(want - have),
                                          pdMS_TO_TICKS(CHUNK_TIMEOUT_MS));
            if (n <= 0) {
                uart_flush_input(k_port);
                reply("VRX ERR delai %s %u\n", name, (unsigned)(got + have));
                return false;
            }
            have += (size_t)n;
        }
        got += want;
        reply("VRX ACK %u\n", (unsigned)got);
    }
    s_last_us = esp_timer_get_time();
    return true;
}

/* Test K19 : his receipt in memory then adds to the signal of the microphones of the voice listen (no writing on the card) */
static void cmd_inject(const char *args)
{
    char crc_hex[16];
    unsigned long size = 0;
    if (!s_session || sscanf(args, "%lu %15s", &size, crc_hex) != 2 || size < 2 || size > FILE_MAX) {
        reply("VRX ERR arguments\n");
        return;
    }
    const uint32_t expected = (uint32_t)strtoul(crc_hex, NULL, 16);
    reply("VRX READY inject\n");
    if (!receive("inject", size)) {
        return;
    }
    const uint32_t crc = crc32(s_buf, size);
    if (crc != expected) {
        reply("VRX ERR crc inject %08" PRIx32 "\n", crc);
        return;
    }
    if (!voice_cmd_inject(s_buf, size)) {
        reply("VRX ERR inject ecoute arretee\n");
        return;
    }
    reply("VRX OK inject %lu %08" PRIx32 "\n", size, crc);
}

/* Full path of a bank file, in the subfolder chosen by vrx_dir */
static void voice_path(const char *name, char *out, size_t size)
{
    if (s_subdir[0] == '\0') {
        snprintf(out, size, SD_VOICE_DIR "/%s", name);
    } else {
        snprintf(out, size, SD_VOICE_DIR "/%s/%s", s_subdir, name);
    }
}

/* "vrx_dir in": vrx_put next write in /glycemie/audio/voice/en (cree au besoin).
 * "vrx_dir read" : verification alone, the file is PAS creates and must already contain files.
 * "vrx_dir -" revient a la racine, ou vit la banque francaise. */
static void cmd_dir(const char *args)
{
    char code[8];
    char mode[12] = "";
    if (!s_session || sscanf(args, "%7s %11s", code, mode) < 1) {
        reply("VRX ERR arguments\n");
        return;
    }
    const bool lecture = strcmp(mode, "lecture") == 0;
    if (strcmp(code, "-") == 0) {
        s_subdir[0] = '\0';
        reply("VRX DIR racine\n");
        return;
    }
    for (const char *p = code; *p != '\0'; p++) {
        if (*p < 'a' || *p > 'z') { /* code ISO in tiny only: never ".." nor "/" */
            reply("VRX ERR arguments\n");
            return;
        }
    }
    if (lecture) {
        /* no writing: the folder must already exist and contain files */
        uint32_t free_mb = 0;
        if (sd_storage_list_voice(code, NULL, NULL, &free_mb) <= 0) {
            reply("VRX ERR dossier %s absent ou vide\n", code);
            return;
        }
        snprintf(s_subdir, sizeof(s_subdir), "%s", code);
        reply("VRX DIR %s lecture\n", code);
        return;
    }
    char path[96];
    snprintf(path, sizeof(path), SD_VOICE_DIR "/%s", code);
    if (sd_storage_mkdir(path) != ESP_OK) {
        reply("VRX ERR dossier %s\n", code);
        return;
    }
    snprintf(s_subdir, sizeof(s_subdir), "%s", code);
    reply("VRX DIR %s\n", code);
}

/* Check file size and CRC before committing a received WAV. */
static void cmd_put(const char *args)
{
    char name[48];
    char crc_hex[16];
    unsigned long size = 0;
    if (!s_session || sscanf(args, "%47s %lu %15s", name, &size, crc_hex) != 3 || !valid_name(name) || size < 44 ||
            size > FILE_MAX) {
        reply("VRX ERR arguments\n");
        return;
    }
    const uint32_t expected = (uint32_t)strtoul(crc_hex, NULL, 16);
    char path[96];
    voice_path(name, path, sizeof(path));
    size_t len = 0;
    if (sd_storage_read_file(path, s_check, FILE_MAX, &len) == ESP_OK && len == size && crc32(s_check, len) == expected) {
        reply("VRX SKIP %s\n", name);
        return;
    }
    reply("VRX READY %s\n", name);
    if (!receive(name, size)) {
        return;
    }
    const uint32_t crc = crc32(s_buf, size);
    if (crc != expected) {
        reply("VRX ERR crc %s %08" PRIx32 "\n", name, crc);
        return;
    }
    if (sd_storage_write_file(path, s_buf, size) != ESP_OK) {
        reply("VRX ERR ecriture %s\n", name);
        return;
    }
    if (sd_storage_read_file(path, s_check, FILE_MAX, &len) != ESP_OK || len != size || crc32(s_check, len) != crc) {
        reply("VRX ERR relecture %s\n", name);
        return;
    }
    reply("VRX OK %s %lu %08" PRIx32 "\n", name, size, crc);
}

static void list_file(const char *name, long size, void *arg)
{
    list_ctx_t *ctx = arg;
    ctx->files++;
    ctx->bytes += size > 0 ? (uint64_t)size : 0;
    if (!ctx->crc) {
        reply("VRX FILE %s %ld\n", name, size);
        return;
    }
    char path[300];
    voice_path(name, path, sizeof(path));
    size_t len = 0;
    if (s_check != NULL && sd_storage_read_file(path, s_check, FILE_MAX, &len) == ESP_OK) {
        reply("VRX FILE %s %ld %08" PRIx32 "\n", name, size, crc32(s_check, len));
    } else {
        reply("VRX FILE %s %ld illisible\n", name, size);
    }
}

/* List the requested language directory with CRCs for read-only verification. */
static void cmd_list(const char *args)
{
    list_ctx_t ctx = {.crc = strcmp(args, "crc") == 0};
    if (ctx.crc && s_check == NULL) {
        s_check = heap_caps_malloc(FILE_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    uint32_t free_mb = 0;
    /* Le listage suit le dossier choisi par vrx_dir : verification en LECTURE SEULE de n'importe quelle langue */
    const int count = sd_storage_list_voice(s_subdir, list_file, &ctx, &free_mb);
    if (count < 0) {
        reply("VRX LIST ERR microSD indisponible\n");
        return;
    }
    reply("VRX LIST dossier=%s fichiers=%d octets=%" PRIu64 " libre_mo=%" PRIu32 "\n",
          s_subdir[0] != '\0' ? s_subdir : "racine", ctx.files, ctx.bytes, free_mb);
    if (!s_session) {
        free(s_check);
        s_check = NULL;
    }
}

bool voice_rx_command(const char *line)
{
    if (strncmp(line, "vrx_", 4) != 0) {
        return false;
    }
    s_last_us = esp_timer_get_time();
    const char *args = strchr(line, ' ');
    args = args != NULL ? args + 1 : "";
    if (strncmp(line, "vrx_begin", 9) == 0) {
        cmd_begin(args);
    } else if (strcmp(line, "vrx_ping") == 0) {
        reply("VRX PONG\n");
    } else if (strncmp(line, "vrx_dir ", 8) == 0) {
        cmd_dir(args);
    } else if (strncmp(line, "vrx_put ", 8) == 0) {
        cmd_put(args);
    } else if (strncmp(line, "vrx_list", 8) == 0) {
        cmd_list(args);
    } else if (strncmp(line, "vrx_inject ", 11) == 0) {
        cmd_inject(args);
    } else if (strcmp(line, "vrx_end") == 0) {
        reply("VRX END\n");
        if (s_session) {
            end_session("vrx_end");
        }
    } else {
        reply("VRX ERR commande\n");
    }
    return true;
}

void voice_rx_idle(void)
{
    if (s_session && esp_timer_get_time() - s_last_us > IDLE_END_US) {
        end_session("30 s sans commande");
    }
}
