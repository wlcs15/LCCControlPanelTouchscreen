#ifndef S3_CDI_WELLFORMED_H
#define S3_CDI_WELLFORMED_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static int s3_cdi_has(const char *xml, unsigned len, const char *needle) {
  unsigned n;
  unsigned i;
  if (xml == 0 || needle == 0) {
    return 0;
  }
  n = (unsigned)strlen(needle);
  if (n == 0 || n > len) {
    return 0;
  }
  for (i = 0; i + n <= len; i++) {
    if (memcmp(xml + i, needle, n) == 0) {
      return 1;
    }
  }
  return 0;
}

static int s3_cdi_configure_ready(const char *xml, unsigned len) {
  if (xml == 0 || len < 16) {
    return 0;
  }
  if (!s3_cdi_has(xml, len, "<cdi")) {
    return 0;
  }
  if (!s3_cdi_has(xml, len, "</cdi>")) {
    return 0;
  }
  if (!s3_cdi_has(xml, len, "<manufacturer>")) {
    return 0;
  }
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif
