// clang-format off
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "dbmanager_conf.h"
#include "src/db_manager.h"
#include "src/http_server.h"
#include "src/logger.h"
// clang-format on

#define DEFAULT_MAX_POOL_SIZE 1

typedef struct command_op {
  char *db_host;
  char *db_user;
  char *db_password;
  char *db_name;
  int pool_size;
  bool usage;
} command_op_t;

/**
 * @brief 输出使用信息
 *
 * @param program_name 程序名
 */
static void print_usage(const char *program_name) {
  printf("Usage:   %s [options]\n", program_name);
  printf("Version: %s\n", OHNO_VERSION);
  printf("Options:\n");
  printf("  --help, -h          Show this help message\n");
  printf("  --db-host=HOST      Database host\n");
  printf("  --db-user=USER      Database user\n");
  printf("  --db-password=PASS  Database password\n");
  printf("  --db-name=NAME      Database name\n");
  printf("  --pool-size=SIZE    Database connection pool size (default: %d)\n",
         DEFAULT_MAX_POOL_SIZE);
}

/**
 * @brief 解析命令行参数
 *
 * @param argc 参数个数
 * @param argv 参数值
 * @param op 结构体保存命令行参数
 * @return int 成功（0）；失败（-1）
 */
static int parse_command(int argc, char **argv, command_op_t *op) {
  int option_index = 0;
  int c;

  static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                         {"db-host", required_argument, 0, 'H'},
                                         {"db-user", required_argument, 0, 'u'},
                                         {"db-password", required_argument, 0, 'p'},
                                         {"db-name", required_argument, 0, 'n'},
                                         {"pool-size", required_argument, 0, 's'},
                                         {0, 0, 0, 0}};

  op->db_host = NULL;
  op->db_user = NULL;
  op->db_password = NULL;
  op->db_name = NULL;
  op->pool_size = DEFAULT_MAX_POOL_SIZE;
  op->usage = false;

  while ((c = getopt_long(argc, argv, "hH:u:p:n:s:", long_options, &option_index)) != -1) {
    switch (c) {
    case 'h':
      op->usage = true;
      return 0;
    case 'H':
      op->db_host = optarg;
      break;
    case 'u':
      op->db_user = optarg;
      break;
    case 'p':
      op->db_password = optarg;
      break;
    case 'n':
      op->db_name = optarg;
      break;
    case 's':
      op->pool_size = atoi(optarg);
      break;
    case '?':
      return -1;
    default:
      return -1;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  command_op_t op;
  if (parse_command(argc, argv, &op) != 0) {
    return EXIT_FAILURE;
  }
  if (op.usage) {
    print_usage(argv[0]);
    return EXIT_SUCCESS;
  }

  if (logger_init(LOG_CONF) == -1) {
    fprintf(stderr, "log initialization failed with config: %s\n", LOG_CONF);
    logger_fini();
    return EXIT_FAILURE;
  }

  LOG_INFO("Starting DB Manager Daemon v%s", OHNO_VERSION);
  db_manager_t *db_mgr =
      db_manager_init(op.db_host, op.db_user, op.db_password, op.db_name, op.pool_size);
  if (!db_mgr) {
    LOG_ERROR("Failed to initialize database manager");
    logger_fini();
    return EXIT_FAILURE;
  }

  http_server_t *http_server = http_server_init(db_mgr);
  if (!http_server) {
    LOG_ERROR("Failed to initialize HTTP server");
    db_manager_destroy(db_mgr);
    logger_fini();
    return EXIT_FAILURE;
  }

  // 启动 HTTP 服务器
  if (http_server_start(http_server) != 0) {
    LOG_ERROR("Failed to start HTTP server");
    http_server_destroy(http_server);
    db_manager_destroy(db_mgr);
    logger_fini();
    return EXIT_FAILURE;
  }

  LOG_INFO("DB Manager HTTP server is running on port %d", HTTP_PORT);

  pause();

  http_server_destroy(http_server);
  db_manager_destroy(db_mgr);
  logger_fini();

  LOG_INFO("DB Manager Daemon stopped");
  return EXIT_SUCCESS;
}
