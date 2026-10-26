/*
 * logging.h - Small leveled logger with a pluggable sink.
 *
 * The engine logs through these functions. The GUI installs a sink so every
 * line also appears, colored by level, in the on-screen log pane. A file sink
 * can be installed in parallel to write the dump session log.
 */
#ifndef UDT_LOGGING_H
#define UDT_LOGGING_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} log_level;

/* A sink receives one already-formatted, newline-free message per call.
 * user is the pointer passed to log_set_sink(). Sinks must be thread-safe
 * if logging happens from a worker thread (it does during a dump). */
typedef void (*log_sink_fn)(void *user, log_level level, const char *msg);

void log_set_sink(log_sink_fn fn, void *user);
void log_set_min_level(log_level level);     /* messages below this are dropped */
log_level log_get_min_level(void);

void log_msg(log_level level, const char *fmt, ...);
void log_vmsg(log_level level, const char *fmt, va_list ap);

#define LOGD(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define LOGI(...) log_msg(LOG_INFO,  __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN,  __VA_ARGS__)
#define LOGE(...) log_msg(LOG_ERROR, __VA_ARGS__)

const char *log_level_name(log_level level);

#ifdef __cplusplus
}
#endif

#endif /* UDT_LOGGING_H */
