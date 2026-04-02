#pragma once

#include <stdio.h>
#include <time.h>

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_ERROR 2

#ifndef LOG_LEVEL
#define LOG LEVEL LOG_LEVEL_DEBUG
#endif

#define LOG_TIMESTAMP(buf)                                                                         \
        do {                                                                                       \
                time_t _t = time(NULL);                                                            \
                struct tm *_tm = localtime(&_t);                                                   \
                strftime(buf, sizeof(buf), "%H:%M:%S", _tm);                                       \
        } while (0)

#define LOG_MESSAGE(level_str, stream, fmt, ...)                                                   \
        do {                                                                                       \
                char _ts[9];                                                                       \
                LOG_TIMESTAMP(_ts);                                                                \
                fprintf(stream, "[%s] [%s] (%s:%d) " fmt "\n", _ts, level_str, __FILE__, __LINE__, \
                        ##__VA_ARGS__);                                                            \
        } while (0)

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define DEBUG(fmt, ...) LOG_MESSAGE("DEBUG", stdout, fmt, ##__VA_ARGS__);
#else
#define DEBUG(FMT, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define INFO(fmt, ...) LOG_MESSAGE("INFO", stdout, fmt, ##__VA_ARGS__);
#else
#define INFO(FMT, ...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define ERROR(fmt, ...) LOG_MESSAGE("ERROR", stdout, fmt, ##__VA_ARGS__);
#else
#define ERROR(FMT, ...) ((void)0)
#endif
