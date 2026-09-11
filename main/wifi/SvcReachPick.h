#ifndef S3_SVC_REACH_PICK_H
#define S3_SVC_REACH_PICK_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* JMRI glass: Wi-Fi up and web TCP reachable. Else fail (same as probe_task). */
static int s3_jmri_icon_ok(int wifi_up, int jmri_tcp)
{
    return (wifi_up && jmri_tcp) ? 1 : 0;
}

/* CAN glass: another node heard on TWAI. */
static int s3_can_icon_ok(int wired)
{
    return wired ? 1 : 0;
}

/* LCC glass: wired bus or Wi-Fi GridConnect. */
static int s3_lcc_icon_ok(int wired, int wifi_lcc)
{
    if (wired)
    {
        return 1;
    }
    return wifi_lcc ? 1 : 0;
}

static int s3_host_usable(const char *host)
{
    if (host == 0 || host[0] == '\0')
    {
        return 0;
    }
    if (strncmp(host, "127.", 4) == 0 || strncmp(host, "0.", 2) == 0)
    {
        return 0;
    }
    if (strncmp(host, "169.254.", 8) == 0)
    {
        return 0;
    }
    return 1;
}

/* Hub IPv4 first, then Kconfig monitor host if different. */
static int s3_web_hosts(const char *hub_ip, const char *monitor,
                        char out[][16], int maxn)
{
    int n = 0;
    if (out == 0 || maxn <= 0)
    {
        return 0;
    }
    if (s3_host_usable(hub_ip) && n < maxn)
    {
        strncpy(out[n], hub_ip, 15);
        out[n][15] = '\0';
        n++;
    }
    if (s3_host_usable(monitor) && n < maxn)
    {
        if (n == 0 || strcmp(out[0], monitor) != 0)
        {
            strncpy(out[n], monitor, 15);
            out[n][15] = '\0';
            n++;
        }
    }
    return n;
}

#ifdef __cplusplus
}
#endif

#endif
