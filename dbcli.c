// clang-format off
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbmanager_conf.h"
#include "src/http_client.h"
#include "src/http_server.h"
#include "src/key.h"
#include "src/macro.h"
// clang-format on

#define DEFAULT_BASE_URL "http://localhost:" STR_HELPER(HTTP_PORT)

typedef struct command_op {
  char *table;
  char *data;
  char *where;
  char *url;
  bool usage;
} command_op_t;

/**
 * @brief 输出 usage
 *
 * @param program_name 程序名
 */
static void print_usage(const char *program_name) {
  printf("Usage:   %s <operation> [options]\n", program_name);
  printf("Version: %s\n", OHNO_VERSION);
  printf("Operations:\n");
  printf("  create --table=TABLE --data=DATA\n");
  printf("  read   --table=TABLE [--where=WHERE]\n");
  printf("  update --table=TABLE --data=DATA --where=WHERE\n");
  printf("  delete --table=TABLE --where=WHERE\n");
  printf("\nOptions:\n");
  printf("  --help, -h    Show this help message\n");
  printf("  --url=URL     HTTP server URL (default: %s)\n", DEFAULT_BASE_URL);
}

/**
 * @brief 解析命令行参数
 *
 * @param argc 命令行个数
 * @param argv 命令行选项
 * @param op 结构体保存命令行参数
 * @return int 成功（0）；失败（-1）
 */
static int parse_command(int argc, char **argv, command_op_t *op) {
  op->table = NULL;
  op->data = NULL;
  op->where = NULL;
  op->url = DEFAULT_BASE_URL;
  op->usage = false;

  // 解析命令行参数
  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},       {"table", required_argument, 0, 't'},
      {"data", required_argument, 0, 'd'}, {"where", required_argument, 0, 'w'},
      {"url", required_argument, 0, 'u'},  {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc - 1, argv + 1, "ht:d:w:u:", long_options, NULL)) != -1) {
    switch (opt) {
    case 'h':
      op->usage = true;
      return 0;
    case 't':
      op->table = optarg;
      break;
    case 'd':
      op->data = optarg;
      break;
    case 'w':
      op->where = optarg;
      break;
    case 'u':
      op->url = optarg;
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
  if (argc == 2) {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    }
  }
  if (argc < 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  const char *operation = argv[1];
  command_op_t op;
  if (parse_command(argc, argv, &op) != 0) {
    return EXIT_FAILURE;
  }
  if (op.usage) {
    print_usage(argv[0]);
    return EXIT_SUCCESS;
  }

  // 初始化 HTTP 客户端
  http_client_t *client = http_client_init(op.url);
  if (!client) {
    return EXIT_FAILURE;
  }

  // 执行相应操作
  int result = -1;
  char *output = NULL;
  if (strcmp(operation, KEY_OP_CREATE) == 0) {
    if (!op.table || !op.data) {
      fprintf(stderr, "Create operation requires --table or --data\n");
    } else {
      result = http_client_create(client, op.table, op.data, &output);
      if (result >= 0) {
        if (output) {
          printf("%s\n", output);
        } else {
          printf("Created %d row(s)\n", result);
        }
      } else {
        if (output) {
          fprintf(stderr, "%s\n", output);
        } else {
          fprintf(stderr, "Create operation failed\n");
        }
      }
    }
  } else if (strcmp(operation, KEY_OP_READ) == 0) {
    if (!op.table) {
      fprintf(stderr, "Read operation requires --table\n");
    } else {
      result = http_client_read(client, op.table, op.where, &output);
      if (result >= 0) {
        if (output) {
          printf("%s\n", output);
        }
      } else {
        if (output) {
          fprintf(stderr, "%s\n", output);
        } else {
          fprintf(stderr, "Read operation failed or no data found\n");
        }
      }
    }
  } else if (strcmp(operation, KEY_OP_UPDATE) == 0) {
    if (!op.table || !op.data || !op.where) {
      fprintf(stderr, "Update operation requires --table, --data or --where\n");
    } else {
      result = http_client_update(client, op.table, op.data, op.where, &output);
      if (result >= 0) {
        if (output) {
          printf("%s\n", output);
        } else {
          printf("Updated %d row(s)\n", result);
        }
      } else {
        if (output) {
          fprintf(stderr, "%s\n", output);
        } else {
          fprintf(stderr, "Update operation failed\n");
        }
      }
    }
  } else if (strcmp(operation, KEY_OP_DELETE) == 0) {
    if (!op.table || !op.where) {
      fprintf(stderr, "Delete operation requires --table or --where\n");
    } else {
      result = http_client_delete(client, op.table, op.where, &output);
      if (result >= 0) {
        if (output) {
          printf("%s\n", output);
        } else {
          printf("Deleted %d row(s)\n", result);
        }
      } else {
        if (output) {
          fprintf(stderr, "%s\n", output);
        } else {
          fprintf(stderr, "Delete operation failed\n");
        }
      }
    }
  } else {
    fprintf(stderr, "Unknown operation: %s\n", operation);
    print_usage(argv[0]);
  }

  if (output != NULL) {
    free(output);
  }
  http_client_cleanup(client);
  return (result >= 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
