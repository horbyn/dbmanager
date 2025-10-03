// clang-format off
#include <stdlib.h>
#include <string.h>
#include "db_manager.h"
#include "src/assert.h"
#include "src/logger.h"
// clang-format on

/**
 * @brief 初始化数据库管理器
 *
 * @param host 主机名字符串
 * @param user 用户名字符串
 * @param password 密码字符串
 * @param database 数据库字符串
 * @param pool_size 连接池大小
 * @return db_manager_t* 数据库管理对象，如果失败返回 NULL
 */
db_manager_t *db_manager_init(const char *host, const char *user, const char *password,
                              const char *database, int pool_size) {
  DBMNGR_ASSERT(host);
  DBMNGR_ASSERT(user);
  DBMNGR_ASSERT(password);
  DBMNGR_ASSERT(database);
  DBMNGR_ASSERT(pool_size > 0);
  LOG_INFO("Initializing DB manager: %s@%s/%s, pool_size=%d", user, host, database, pool_size);

  db_manager_t *manager = malloc(sizeof(db_manager_t));
  if (!manager) {
    LOG_ERROR("Failed to allocate memory for DB manager");
    return NULL;
  }

  manager->conn_pool = create_connection_pool(host, user, password, database, pool_size);
  if (!manager->conn_pool) {
    LOG_ERROR("Failed to create connection pool for DB manager");
    free(manager);
    return NULL;
  }

  manager->last_error = NULL;
  manager->max_retries = DB_MAX_RETRIES;

  LOG_INFO("DB manager initialized successfully");
  return manager;
}

/**
 * @brief 销毁数据库管理器
 *
 * @param manager 数据库管理对象
 */
void db_manager_destroy(db_manager_t *manager) {
  if (!manager) {
    return;
  }

  LOG_INFO("Destroying DB manager");

  if (manager->conn_pool) {
    destroy_connection_pool(manager->conn_pool);
  }

  if (manager->last_error) {
    free(manager->last_error);
  }

  free(manager);
  LOG_INFO("DB manager destroyed");
}

/**
 * @brief 执行数据库操作
 *
 * @param manager 数据库管理对象
 * @param query sql 语句
 * @return mysql_connection_t* 数据库连接对象
 */
static mysql_connection_t *db_manager_execute_common(db_manager_t *manager, const char *query) {
  if (!manager || !query) {
    return NULL;
  }

  mysql_connection_t *conn = NULL;
  int retry_count = 0;

  while (retry_count < manager->max_retries) {
    conn = get_connection(manager->conn_pool);
    if (!conn) {
      LOG_ERROR("Failed to get connection (attempt %d/%d)", retry_count + 1, manager->max_retries);
      ++retry_count;
      continue;
    }

    if (mysql_query(conn->mysql_conn, query) != 0) {
      const char *error_msg = mysql_error(conn->mysql_conn);
      LOG_ERROR("Query execution failed: %s (attempt %d/%d)", error_msg, retry_count + 1,
                manager->max_retries);

      if (manager->last_error) {
        free(manager->last_error);
      }
      manager->last_error = strdup(error_msg);
      release_connection(manager->conn_pool, conn);

      // 连接错误重试
      if (mysql_errno(conn->mysql_conn) == CR_SERVER_GONE_ERROR ||
          mysql_errno(conn->mysql_conn) == CR_SERVER_LOST) {
        retry_count++;
        continue;
      } else {
        break;
      }
    }

    return conn; // 成功执行查询
  }

  return NULL;
}

/**
 * @brief 执行查询并返回结果集（READ）
 *
 * @param manager 数据库管理对象
 * @param query sql 语句
 * @return db_result_t* 结果集对象，如果失败返回 NULL
 */
static db_result_t *db_manager_execute_query(db_manager_t *manager, const char *query) {
  if (!manager || !query) {
    LOG_ERROR("Invalid parameters for execute_query");
    return NULL;
  }

  LOG_DEBUG("Executing query: %s", query);

  mysql_connection_t *conn = db_manager_execute_common(manager, query);
  if (conn == NULL) {
    LOG_ERROR("Query execution failed after %d attempts", manager->max_retries);
    return NULL;
  }

  MYSQL_RES *mysql_res = mysql_store_result(conn->mysql_conn);
  if (!mysql_res && mysql_field_count(conn->mysql_conn) > 0) {
    // 应该有结果集但没有获取到
    const char *error_msg = mysql_error(conn->mysql_conn);
    LOG_ERROR("Failed to store result: %s", error_msg);

    if (manager->last_error) {
      free(manager->last_error);
    }
    manager->last_error = strdup(error_msg);
    release_connection(manager->conn_pool, conn);
    return NULL;
  }

  db_result_t *result = malloc(sizeof(db_result_t));
  if (!result) {
    LOG_ERROR("Failed to allocate memory for result");
    if (mysql_res) {
      mysql_free_result(mysql_res);
    }
    release_connection(manager->conn_pool, conn);
    return NULL;
  }

  result->mysql_res = mysql_res;
  result->num_rows = mysql_res ? mysql_num_rows(mysql_res) : 0;
  result->num_fields = mysql_res ? mysql_num_fields(mysql_res) : 0;

  release_connection(manager->conn_pool, conn);

  LOG_DEBUG("Query executed successfully, %d rows returned", result->num_rows);
  return result;
}

/**
 * @brief 执行更新操作（INSERT, UPDATE, DELETE）
 *
 * @param manager 数据库管理对象
 * @param query sql 语句
 * @return int 已生效条目数量
 */
static int db_manager_execute_update(db_manager_t *manager, const char *query) {
  if (!manager || !query) {
    LOG_ERROR("Invalid parameters for execute_update");
    return -1;
  }

  LOG_DEBUG("Executing update: %s", query);

  mysql_connection_t *conn = db_manager_execute_common(manager, query);
  if (conn == NULL) {
    LOG_ERROR("Update execution failed after %d attempts", manager->max_retries);
    return -1;
  }

  int affected_rows = mysql_affected_rows(conn->mysql_conn);
  release_connection(manager->conn_pool, conn);

  LOG_DEBUG("Update executed successfully, %lld rows affected", (long long)affected_rows);
  return affected_rows;
}

/**
 * @brief 释放结果集
 *
 * @param result 结果集对象
 */
void db_result_free(db_result_t *result) {
  if (!result)
    return;

  if (result->mysql_res) {
    mysql_free_result(result->mysql_res);
  }

  free(result);
}

/**
 * @brief 执行插入操作（INSERT）
 *
 * @param manager 数据库管理对象
 * @param table 表
 * @param data 数据
 * @return int 生效条目数量
 */
int db_manager_create_row(db_manager_t *manager, const char *table, const char *data) {
  if (!manager || !table || !data) {
    LOG_ERROR("Invalid parameters for create_row");
    return -1;
  }

  char query[1024];
  snprintf(query, sizeof(query), "INSERT INTO %s SET %s", table, data);

  LOG_INFO("Creating row in %s: %s", table, data);
  return db_manager_execute_update(manager, query);
}

/**
 * @brief 执行查询操作（SELECT）
 *
 * @param manager 数据库管理对象
 * @param table 表
 * @param where 条件
 * @return db_result_t* 结果集
 */
db_result_t *db_manager_read_row(db_manager_t *manager, const char *table, const char *where) {
  if (!manager || !table) {
    LOG_ERROR("Invalid parameters for read_row");
    return NULL;
  }

  char query[1024];
  if (where && where[0] != '\0') {
    snprintf(query, sizeof(query), "SELECT * FROM %s WHERE %s", table, where);
  } else {
    snprintf(query, sizeof(query), "SELECT * FROM %s", table);
  }

  LOG_INFO("Reading from %s with condition: %s", table, where ? where : "none");
  return db_manager_execute_query(manager, query);
}

/**
 * @brief 执行更新操作（UPDATE）
 *
 * @param manager 数据库管理对象
 * @param table 表
 * @param data 数据
 * @param where 条件
 * @return int 生效条目数
 */
int db_manager_update_row(db_manager_t *manager, const char *table, const char *data,
                          const char *where) {
  if (!manager || !table || !data || !where) {
    LOG_ERROR("Invalid parameters for update_row");
    return -1;
  }

  char query[1024];
  snprintf(query, sizeof(query), "UPDATE %s SET %s WHERE %s", table, data, where);

  LOG_INFO("Updating %s: SET %s WHERE %s", table, data, where);
  return db_manager_execute_update(manager, query);
}

/**
 * @brief 执行删除操作
 *
 * @param manager 数据库管理对象
 * @param table 表
 * @param where 条件
 * @return int 生效条目数量
 */
int db_manager_delete_row(db_manager_t *manager, const char *table, const char *where) {
  if (!manager || !table || !where) {
    LOG_ERROR("Invalid parameters for delete_row");
    return -1;
  }

  char query[1024];
  snprintf(query, sizeof(query), "DELETE FROM %s WHERE %s", table, where);

  LOG_INFO("Deleting from %s WHERE %s", table, where);
  return db_manager_execute_update(manager, query);
}
