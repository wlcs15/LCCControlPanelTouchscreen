#ifndef RR_GIT_VERSION_H
#define RR_GIT_VERSION_H

// Compile-time SNIP software version. scripts/git_version.py bakes
// -DRR_GIT_VERSION=v2.04 or v2.04+ (tokens, then stringized). Fallback
// when Arduino IDE compiles without that flag, or git has no well-formed
// vN.N tag: v0.01+
#ifndef RR_GIT_VERSION
#define RR_GIT_VERSION v0.01+
#endif

#define RR_GIT_VERSION_XSTR(x) #x
#define RR_GIT_VERSION_STR(x) RR_GIT_VERSION_XSTR(x)

#endif
