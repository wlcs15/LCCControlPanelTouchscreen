#include "sd_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LOG_PATH "/sdcard/debug.log"
#define LOG_PREV "/sdcard/debug.prev.log"
#define LOG_MAX_BYTES (192 * 1024)

static FILE *s_fp;
static SemaphoreHandle_t s_mu;
static volatile int s_busy;
static int s_unflushed;
static vprintf_like_t s_prev;

static const char *reset_name(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
#ifdef ESP_RST_USB
    case ESP_RST_USB:       return "USB";
#endif
    default:                return "OTHER";
    }
}

static void rotate_if_needed(void)
{
    struct stat st;
    if (stat(LOG_PATH, &st) != 0 || st.st_size <= LOG_MAX_BYTES) {
        return;
    }
    unlink(LOG_PREV);
    rename(LOG_PATH, LOG_PREV);
}

static int sd_vprintf(const char *fmt, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);
    const int n = s_prev ? s_prev(fmt, ap) : vprintf(fmt, ap);

    if (s_fp && !s_busy && s_mu && xSemaphoreTake(s_mu, 0) == pdTRUE) {
        s_busy = 1;
        (void)vfprintf(s_fp, fmt, ap2);
        if (++s_unflushed >= 12) {
            fflush(s_fp);
            fsync(fileno(s_fp));
            s_unflushed = 0;
        }
        s_busy = 0;
        xSemaphoreGive(s_mu);
    }
    va_end(ap2);
    return n;
}

static void on_shutdown(void)
{
    sd_log_flush();
}

void sd_log_init(void)
{
    rotate_if_needed();
    if (!s_mu) {
        s_mu = xSemaphoreCreateMutex();
    }
    if (!s_mu) {
        return;
    }

    s_fp = fopen(LOG_PATH, "a");
    if (!s_fp) {
        return;
    }

    const esp_reset_reason_t rr = esp_reset_reason();
    fprintf(s_fp, "\n==== boot reset=%s (%d) heap=%lu ====\n",
            reset_name(rr), (int)rr, (unsigned long)esp_get_free_heap_size());
    fflush(s_fp);
    fsync(fileno(s_fp));

    s_prev = esp_log_set_vprintf(sd_vprintf);
    (void)esp_register_shutdown_handler(on_shutdown);

    ESP_LOGI("sd_log", "mirroring logs to %s (prev crash reason above)", LOG_PATH);
    if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT ||
        rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT) {
        ESP_LOGE("sd_log", "previous run died: %s", reset_name(rr));
    }
}

void sd_log_flush(void)
{
    if (!s_fp || !s_mu) {
        return;
    }
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(80)) != pdTRUE) {
        return;
    }
    fflush(s_fp);
    fsync(fileno(s_fp));
    s_unflushed = 0;
    xSemaphoreGive(s_mu);
}
