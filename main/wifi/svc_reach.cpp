#include "svc_reach.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcc_node.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"
#include "SvcReachPick.h"
#include "ui_wifi_icon.h"
#include "wifi_sta.h"

static const char *TAG = "svc_reach";

static bool tcp_is_open(const char *host, int port, int timeout_ms)
{
    if (host == nullptr || host[0] == '\0' || port <= 0)
    {
        return false;
    }

    char port_s[16];
    snprintf(port_s, sizeof(port_s), "%d", port);

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    if (getaddrinfo(host, port_s, &hints, &res) != 0 || res == nullptr)
    {
        return false;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0)
    {
        freeaddrinfo(res);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    const int rc = connect(fd, res->ai_addr, res->ai_addrlen);
    close(fd);
    freeaddrinfo(res);
    return rc == 0;
}

static void mark_async(void *param)
{
    const uintptr_t packed = (uintptr_t)param;
    const ui_mark_icon_t st = (ui_mark_icon_t)(packed & 0xF);
    const int which = (int)(packed >> 4);
    if (which == 0)
    {
        ui_jmri_icon_set_state(st);
    }
    else if (which == 1)
    {
        ui_lcc_icon_set_state(st);
    }
    else
    {
        ui_can_icon_set_state(st);
    }
}

static void apply_mark(int which, ui_mark_icon_t st)
{
    const uintptr_t packed = ((uintptr_t)which << 4) | (uintptr_t)st;
    if (lv_is_initialized())
    {
        lv_async_call(mark_async, (void *)packed);
    }
}

static int probe_jmri_web(int wifi_up, const char *hub)
{
    if (!wifi_up)
    {
        return 0;
    }
    if (hub && hub[0] && tcp_is_open(hub, CONFIG_JMRI_WEB_PORT, 1500))
    {
        return 1;
    }
    if (CONFIG_JMRI_WEB_HOST[0] &&
        tcp_is_open(CONFIG_JMRI_WEB_HOST, CONFIG_JMRI_WEB_PORT, 1500))
    {
        return 1;
    }
    return 0;
}

static void apply_reach_icons(int wifi_up, int jmri, int wired, int wifi_lcc)
{
    apply_mark(0, s3_jmri_icon_ok(wifi_up, jmri) ? UI_JMRI_ICON_OK
                                                 : UI_JMRI_ICON_FAIL);
    apply_mark(2, s3_can_icon_ok(wired) ? UI_JMRI_ICON_OK : UI_JMRI_ICON_FAIL);
    apply_mark(1, s3_lcc_icon_ok(wired, wifi_lcc) ? UI_JMRI_ICON_OK
                                                  : UI_JMRI_ICON_FAIL);
}

static const char *up_down(int ok)
{
    return ok ? "up" : "down";
}

static void log_reach(int wifi_up, int jmri, int wired, int wifi_lcc,
                      const char *hub)
{
    const char *web = "(mdns)";
    const char *lcc_host = "(mdns)";
    const char *ip = wifi_sta_ip();
    if (hub && hub[0])
    {
        web = hub;
        lcc_host = hub;
    }
    else if (CONFIG_JMRI_WEB_HOST[0])
    {
        web = CONFIG_JMRI_WEB_HOST;
    }
    if (!ip[0])
    {
        ip = "-";
    }
    ESP_LOGI(TAG,
             "wifi=%s ip=%s JMRI %s:%d %s CAN %s CS105 %s:%d / Pi %s:%d => LCC %s",
             up_down(wifi_up), ip, web, CONFIG_JMRI_WEB_PORT, up_down(jmri),
             up_down(wired), lcc_host, CONFIG_LCC_WIFI_PORT, "-",
             CONFIG_LCC_WIFI_PORT, up_down(wired || wifi_lcc));
}

static void probe_once(void)
{
    const int wifi_up = (wifi_sta_state() == WIFI_STA_CONNECTED) ? 1 : 0;
    const int wired = lcc_node_wired_link_ok() ? 1 : 0;
    const char *hub = lcc_node_wifi_hub_ip();
    const int wifi_lcc = (wifi_up && lcc_node_wifi_hub_ok()) ? 1 : 0;
    const int jmri = probe_jmri_web(wifi_up, hub);
    apply_reach_icons(wifi_up, jmri, wired, wifi_lcc);
    log_reach(wifi_up, jmri, wired, wifi_lcc, hub);
}

static void probe_task(void *arg)
{
    (void)arg;
    apply_mark(0, UI_JMRI_ICON_OFF);
    apply_mark(1, UI_JMRI_ICON_OFF);
    apply_mark(2, UI_JMRI_ICON_OFF);
    for (;;)
    {
        probe_once();
        vTaskDelay(pdMS_TO_TICKS(
            (wifi_sta_state() == WIFI_STA_CONNECTED) ? 8000 : 2000));
    }
}

void svc_reach_start(void)
{
    static bool started = false;
    if (started)
    {
        return;
    }
    started = true;
    xTaskCreate(probe_task, "svc_reach", 4096, nullptr, 4, nullptr);
}
