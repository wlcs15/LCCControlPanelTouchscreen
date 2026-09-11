#include "SvcReachPick.h"
#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect_eq(int got, int want, const char *name)
{
    if (got != want)
    {
        printf("FAIL %s got %d want %d\n", name, got, want);
        g_fail++;
    }
}

int main(void)
{
    char hosts[2][16];

    expect_eq(s3_jmri_icon_ok(1, 1), 1, "jmri wifi+tcp");
    expect_eq(s3_jmri_icon_ok(1, 0), 0, "jmri wifi no tcp");
    expect_eq(s3_jmri_icon_ok(0, 1), 0, "jmri tcp no wifi");
    expect_eq(s3_can_icon_ok(1), 1, "can wired");
    expect_eq(s3_can_icon_ok(0), 0, "can down");
    expect_eq(s3_lcc_icon_ok(1, 0), 1, "lcc wired");
    expect_eq(s3_lcc_icon_ok(0, 1), 1, "lcc wifi");
    expect_eq(s3_lcc_icon_ok(0, 0), 0, "lcc neither");
    expect_eq(s3_host_usable(""), 0, "empty");
    expect_eq(s3_host_usable("127.0.0.1"), 0, "loopback");
    expect_eq(s3_host_usable("169.254.1.1"), 0, "linklocal");
    expect_eq(s3_host_usable("192.168.1.82"), 1, "lan");
    expect_eq(s3_web_hosts(0, 0, hosts, 2), 0, "null hosts");
    expect_eq(s3_web_hosts("192.168.1.82", "192.168.1.57", hosts, 2), 2,
              "dual home");
    expect_eq(strcmp(hosts[0], "192.168.1.82") == 0, 1, "hub first");
    expect_eq(s3_web_hosts("192.168.1.82", "192.168.1.82", hosts, 2), 1,
              "dedupe");
    expect_eq(s3_web_hosts("", "192.168.1.57", hosts, 1), 1, "monitor only");
    expect_eq(s3_web_hosts("x", "y", 0, 2), 0, "no out");
    expect_eq(s3_host_usable("0.0.0.0"), 0, "zero");
    expect_eq(s3_web_hosts("192.168.1.82", "192.168.1.57", hosts, 1), 1,
              "cap 1");

    if (g_fail)
    {
        printf("%d FAIL\n", g_fail);
        return 1;
    }
    printf("host SvcReachPick OK\n");
    return 0;
}
