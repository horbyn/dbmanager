// clang-format off
#include "logger.h"
// clang-format on

/**
 * @brief 初始化日志对象
 *
 * @param config_path 配置文件路径名
 * @return int 成功返回 0，失败返回 -1
 */
int logger_init(const char *config_path) {
  int rc = dzlog_init(config_path, LOG_CATEGORY_DEFAULT);
  if (rc) {
    return -1;
  }
  return 0;
}

/**
 * @brief 释放日志对象
 *
 */
void logger_fini(void) { zlog_fini(); }
