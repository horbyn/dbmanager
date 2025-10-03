#pragma once

// clang-format off
#include <pthread.h>
#include <stdbool.h>
#include <mysql/mysql.h>
// clang-format on

typedef struct {
  MYSQL *mysql_conn;
  time_t last_used_time;
  bool in_use;
  int connection_id;
  char *host;
  char *user;
  char *password;
  char *database;
  unsigned int port;
} mysql_connection_t;

typedef struct {
  mysql_connection_t *connections; // 共享资源，数组形式组织
  int pool_size;
  int active_connections;
  pthread_mutex_t pool_mutex;
  pthread_cond_t connection_available;
  bool shutdown;
} connection_pool_t;

connection_pool_t *create_connection_pool(const char *host, const char *user, const char *password,
                                          const char *database, int pool_size);
mysql_connection_t *get_connection(connection_pool_t *pool);
void release_connection(connection_pool_t *pool, mysql_connection_t *conn);
void destroy_connection_pool(connection_pool_t *pool);
bool check_connection_health(mysql_connection_t *conn);
