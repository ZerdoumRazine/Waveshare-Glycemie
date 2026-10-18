/* Bouchons des modules non testes ici (reseau, microSD) : aucune ecriture reelle. */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"

static int s_time_valid = 1;
void test_set_time_valid(int ok) { s_time_valid = ok; }
int test_sd_lines(void);

bool net_time_is_valid(void) { return s_time_valid != 0; }

static int s_sd_lines;
int test_sd_lines(void) { return s_sd_lines; }
void test_reset_sd_lines(void) { s_sd_lines = 0; }

esp_err_t sd_storage_append_line(const char *path, const char *header, const char *line)
{
    (void)path; (void)header; (void)line;
    s_sd_lines++;
    return ESP_OK;
}

void sd_storage_log_event(const char *fmt, ...) { (void)fmt; }
