#include "appconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#  include <windows.h>
#endif

static void trim(char *s)
{
    char *p = s;
    size_t n;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static void strip_comment(char *s)
{
    char *c;
    for (c = s; *c; c++) {
        if (*c == ';' || *c == '#') { *c = '\0'; break; }
    }
}

static uint64_t parse_u64(const char *v)
{
    if (!v) return 0;
    while (*v && isspace((unsigned char)*v)) v++;
    if (v[0] == '0' && (v[1] == 'x' || v[1] == 'X'))
        return (uint64_t)strtoull(v + 2, NULL, 16);
    return (uint64_t)strtoull(v, NULL, 10);
}

int appconfig_load(const char *ini_path, app_config *cfg)
{
    FILE *f;
    char line[640];

    memset(cfg, 0, sizeof(*cfg));
    cfg->base_addr = 0;
    cfg->chunk = 1024;
    cfg->baud = 115200;
    cfg->method = 0;

    if (!ini_path) return 0;
    f = fopen(ini_path, "r");
    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *val;
        strip_comment(line);
        trim(line);
        if (line[0] == '\0' || line[0] == '[') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = line;
        val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "size") == 0) {
            cfg->flash_size = parse_u64(val);
            cfg->have_size = cfg->flash_size != 0;
        } else if (strcmp(key, "base_addr") == 0) {
            cfg->base_addr = (uint32_t)parse_u64(val);
        } else if (strcmp(key, "chunk") == 0) {
            cfg->chunk = (uint32_t)parse_u64(val);
        } else if (strcmp(key, "baud") == 0) {
            cfg->baud = (uint32_t)parse_u64(val);
        } else if (strcmp(key, "method") == 0) {
            if (strcmp(val, "linear") == 0) cfg->method = 1;
            else if (strcmp(val, "fdl2") == 0) cfg->method = 2;
            else cfg->method = 0;
        } else if (strcmp(key, "fdl1") == 0) {
            snprintf(cfg->fdl1, sizeof(cfg->fdl1), "%s", val);
        } else if (strcmp(key, "fdl1_addr") == 0) {
            cfg->fdl1_addr = (uint32_t)parse_u64(val);
        } else if (strcmp(key, "fdl2") == 0) {
            snprintf(cfg->fdl2, sizeof(cfg->fdl2), "%s", val);
        } else if (strcmp(key, "fdl2_addr") == 0) {
            cfg->fdl2_addr = (uint32_t)parse_u64(val);
        }
    }
    fclose(f);
    return 1;
}

void appconfig_default_path(char *buf, int cap)
{
#if defined(_WIN32)
    char exe[512];
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (n > 0 && n < sizeof(exe)) {
        char *slash = strrchr(exe, '\\');
        if (slash) {
            *slash = '\0';
            snprintf(buf, cap, "%s\\UnisocDumpTool.ini", exe);
            return;
        }
    }
    snprintf(buf, cap, "UnisocDumpTool.ini");
#else
    snprintf(buf, cap, "UnisocDumpTool.ini");
#endif
}
