#pragma once

// clang-format off
#include "connection_pool.h"
// clang-format on

#define DB_MAX_RETRIES 3

typedef struct {
  MYSQL_RES *mysql_res;
  int num_rows;
  int num_fields;
} db_result_t;

typedef struct {
  connection_pool_t *conn_pool;
  char *last_error;
  int max_retries;
} db_manager_t;

db_manager_t *db_manager_init(const char *host, const char *user, const char *password,
                              const char *database, int pool_size);
void db_manager_destroy(db_manager_t *manager);
void db_result_free(db_result_t *result);
int db_manager_create_row(db_manager_t *manager, const char *table, const char *data);
db_result_t *db_manager_read_row(db_manager_t *manager, const char *table, const char *where);
int db_manager_update_row(db_manager_t *manager, const char *table, const char *data,
                          const char *where);
int db_manager_delete_row(db_manager_t *manager, const char *table, const char *where);
