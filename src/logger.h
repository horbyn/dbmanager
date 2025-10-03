#pragma once

// clang-format off
#include <stdio.h>
#include <stdlib.h>
#include "zlog.h"
#include "dbmanager_conf.h"
// clang-format on

#define LOG_CATEGORY_DEFAULT LOG_CATEGORY

int logger_init(const char *config_path);
void logger_fini(void);

#define LOG_FATAL(format, ...) dzlog_fatal(format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) dzlog_error(format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) dzlog_warn(format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) dzlog_info(format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) dzlog_debug(format, ##__VA_ARGS__)
#define LOG_TRACE(format, ...) dzlog_trace(format, ##__VA_ARGS__)
