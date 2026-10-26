#include "logging.h"

#include <stdio.h>

static log_sink_fn g_sink = NULL;
static void       *g_sink_user = NULL;
static log_level   g_min_level = LOG_INFO;

void log_set_sink(log_sink_fn fn, void *user)
{
    g_sink = fn;
    g_sink_user = user;
}

void log_set_min_level(log_level level)
{
    g_min_level = level;
}

log_level log_get_min_level(void)
{
    return g_min_level;
}

const char *log_level_name(log_level level)
{
    switch (level) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_WARN:  return "WARN";
    case LOG_ERROR: return "ERROR";
    default:        return "?";
    }
}

void log_vmsg(log_level level, const char *fmt, va_list ap)
{
    char buf[1024];

    if (level < g_min_level) {
        return;
    }

    vsnprintf(buf, sizeof(buf), fmt, ap);
    buf[sizeof(buf) - 1] = '\0';

    if (g_sink) {
        g_sink(g_sink_user, level, buf);
    } else {
        /* Fallback for headless/test runs. */
        fprintf(stderr, "[%s] %s\n", log_level_name(level), buf);
    }
}

void log_msg(log_level level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_vmsg(level, fmt, ap);
    va_end(ap);
}
