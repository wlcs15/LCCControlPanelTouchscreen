#include "CdiWellFormed.h"
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
    static const char cdi[] =
        "<?xml version=\"1.0\"?>\n"
        "<cdi>\n"
        "<identification>\n"
        "  <manufacturer>OwlThree</manufacturer>\n"
        "  <model>LCC Turnout Panel</model>\n"
        "</identification>\n"
        "<acdi/>\n"
        "<segment space=\"251\" origin=\"1\"></segment>\n"
        "<segment space=\"253\" origin=\"128\"></segment>\n"
        "</cdi>\n";
    static const char truncated[] =
        "<?xml version=\"1.0\"?><cdi><identification>";

    expect_eq(s3_cdi_configure_ready(cdi, static_cast<unsigned>(strlen(cdi))), 1,
              "A5.04 full CDI");
    expect_eq(s3_cdi_configure_ready(
                  truncated, static_cast<unsigned>(strlen(truncated))),
              0, "truncated hides Configure");
    expect_eq(s3_cdi_configure_ready(0, 32), 0, "null xml");
    expect_eq(s3_cdi_configure_ready("short", 5), 0, "too short");
    expect_eq(s3_cdi_has(cdi, static_cast<unsigned>(strlen(cdi)), 0), 0,
              "null needle");
    expect_eq(s3_cdi_has(cdi, static_cast<unsigned>(strlen(cdi)), ""), 0,
              "empty needle");
    static const char noman[] = "<?xml version=\"1.0\"?><cdi></cdi>";
    expect_eq(s3_cdi_configure_ready(noman, static_cast<unsigned>(strlen(noman))),
              0, "no manufacturer");

    if (g_fail)
    {
        printf("%d FAIL\n", g_fail);
        return 1;
    }
    printf("host A5.04 CDI Configure OK\n");
    return 0;
}
