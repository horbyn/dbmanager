// clang-format off
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "connection_pool.h"
#include "src/assert.h"
#include "src/logger.h"
// clang-format on

/**
 * @brief 创建单个 MySQL 连接
 *
 * @param host 主机名字符串
 * @param user 用户名字符串
 * @param password 密码字符串
 * @param database 数据库字符串
 * @param connection_id 唯一标识
 * @return mysql_connection_t* 数据库连接对象
 */
static mysql_connection_t *create_single_connection(const char *host, const char *user,
                                                    const char *password, const char *database,
                                                    int connection_id) {
  DBMNGR_ASSERT(host);
  DBMNGR_ASSERT(user);
  DBMNGR_ASSERT(password);
  DBMNGR_ASSERT(database);
  DBMNGR_ASSERT(connection_id >= 0);
  mysql_connection_t *conn = malloc(sizeof(mysql_connection_t));
  if (!conn) {
    LOG_ERROR("Failed to allocate memory for connection");
    return NULL;
  }

  conn->mysql_conn = mysql_init(NULL);
  if (conn->mysql_conn == NULL) {
    LOG_ERROR("mysql_init() failed for connection %d", connection_id);
    free(conn);
    return NULL;
  }

  if (mysql_real_connect(conn->mysql_conn, host, user, password, database, 0, NULL, 0) == NULL) {
    LOG_ERROR("mysql_real_connect() failed for connection %d: %s", connection_id,
              mysql_error(conn->mysql_conn));
    mysql_close(conn->mysql_conn);
    free(conn);
    return NULL;
  }

  conn->connection_id = connection_id;
  conn->in_use = false;
  conn->last_used_time = time(NULL);
  conn->host = strdup(host);
  conn->user = strdup(user);
  conn->password = strdup(password);
  conn->database = strdup(database);
  conn->port = 0; // 使用默认端口

  LOG_DEBUG("Created MySQL connection %d to %s@%s/%s", connection_id, user, host, database);

  return conn;
}

/**
 * @brief 创建连接池
 *
 * @param host 主机名字符串
 * @param user 用户名字符串
 * @param password 密码字符串
 * @param database 数据库字符串
 * @param pool_size 连接池数量
 * @return connection_pool_t* 连接池对象
 */
connection_pool_t *create_connection_pool(const char *host, const char *user, const char *password,
                                          const char *database, int pool_size) {
  DBMNGR_ASSERT(host);
  DBMNGR_ASSERT(user);
  DBMNGR_ASSERT(password);
  DBMNGR_ASSERT(database);
  DBMNGR_ASSERT(pool_size > 0);

  LOG_INFO("Creating connection pool: size=%d, %s@%s/%s", pool_size, user, host, database);

  connection_pool_t *pool = malloc(sizeof(connection_pool_t));
  if (!pool) {
    LOG_ERROR("Failed to allocate memory for connection pool");
    return NULL;
  }

  pool->connections = malloc(sizeof(mysql_connection_t) * pool_size);
  if (!pool->connections) {
    LOG_ERROR("Failed to allocate memory for connections array");
    free(pool);
    return NULL;
  }

  pool->pool_size = pool_size;
  pool->active_connections = 0;
  pool->shutdown = false;

  if (pthread_mutex_init(&pool->pool_mutex, NULL) != 0) {
    LOG_ERROR("Failed to initialize pool mutex");
    free(pool->connections);
    free(pool);
    return NULL;
  }

  if (pthread_cond_init(&pool->connection_available, NULL) != 0) {
    LOG_ERROR("Failed to initialize condition variable");
    pthread_mutex_destroy(&pool->pool_mutex);
    free(pool->connections);
    free(pool);
    return NULL;
  }

  int successful_connections = 0;
  for (int i = 0; i < pool_size; ++i) {
    pool->connections[i] = *create_single_connection(host, user, password, database, i);
    if (pool->connections[i].mysql_conn != NULL) {
      ++successful_connections;
    } else {
      LOG_WARN("Failed to create connection %d", i);
    }
  }

  if (successful_connections == 0) {
    LOG_ERROR("Failed to create any connections in the pool");
    destroy_connection_pool(pool);
    return NULL;
  }

  LOG_INFO("Connection pool created successfully with %d/%d connections", successful_connections,
           pool_size);

  return pool;
}

/**
 * @brief 获取数据库连接对象
 *
 * @param pool 数据库连接池
 * @return mysql_connection_t* 数据库连接对象
 */
mysql_connection_t *get_connection(connection_pool_t *pool) {
  if (pool == NULL || pool->shutdown) {
    LOG_ERROR("Attempted to get connection from invalid or shutdown pool");
    return NULL;
  }

  pthread_mutex_lock(&pool->pool_mutex);

  while (true) {
    for (int i = 0; i < pool->pool_size; ++i) {
      mysql_connection_t *conn = &pool->connections[i];

      if (!conn->in_use && conn->mysql_conn != NULL) {
        if (!check_connection_health(conn)) {
          LOG_WARN("Connection %d is unhealthy, attempting to reconnect", conn->connection_id);

          // 健康检查失败则重新建立连接
          mysql_close(conn->mysql_conn);
          mysql_connection_t *new_conn = create_single_connection(
              conn->host, conn->user, conn->password, conn->database, conn->connection_id);

          if (new_conn != NULL) {
            *conn = *new_conn;
            free(new_conn);
          } else {
            LOG_ERROR("Failed to reconnect connection %d", conn->connection_id);
            continue; // 跳过这个连接，继续找下一个
          }
        } // end if(健康检查)

        conn->in_use = true;
        conn->last_used_time = time(NULL);
        ++pool->active_connections;

        pthread_mutex_unlock(&pool->pool_mutex);

        LOG_DEBUG("Acquired connection %d, active: %d", conn->connection_id,
                  pool->active_connections);
        return conn;
      } // end if()
    } // end for()

    // 没有可用连接
    LOG_DEBUG("No available connections, waiting...");
    pthread_cond_wait(&pool->connection_available, &pool->pool_mutex);
  } // end while()
}

/**
 * @brief 释放数据库连接
 *
 * @param pool 数据库连接池
 * @param conn 数据库连接对象
 */
void release_connection(connection_pool_t *pool, mysql_connection_t *conn) {
  if (pool == NULL || conn == NULL) {
    return;
  }

  pthread_mutex_lock(&pool->pool_mutex);

  if (conn->in_use) {
    conn->in_use = false;
    conn->last_used_time = time(NULL);
    --pool->active_connections;

    LOG_DEBUG("Released connection %d, active: %d", conn->connection_id, pool->active_connections);

    pthread_cond_signal(&pool->connection_available);
  }

  pthread_mutex_unlock(&pool->pool_mutex);
}

/**
 * @brief 检查连接是否健康
 *
 * @param conn 数据库连接对象
 * @return true 健康
 * @return false 不健康
 */
bool check_connection_health(mysql_connection_t *conn) {
  if (conn == NULL || conn->mysql_conn == NULL) {
    return false;
  }

  if (mysql_ping(conn->mysql_conn) != 0) {
    LOG_WARN("Connection %d ping failed: %s", conn->connection_id, mysql_error(conn->mysql_conn));
    return false;
  }

  return true;
}

/**
 * @brief 销毁连接池
 *
 * @param pool 数据库连接池对象
 */
void destroy_connection_pool(connection_pool_t *pool) {
  if (pool == NULL) {
    return;
  }

  LOG_INFO("Destroying connection pool");

  pthread_mutex_lock(&pool->pool_mutex);
  pool->shutdown = true;

  pthread_cond_broadcast(&pool->connection_available);
  pthread_mutex_unlock(&pool->pool_mutex);

  for (int i = 0; i < pool->pool_size; ++i) {
    mysql_connection_t *conn = &pool->connections[i];

    if (conn->mysql_conn != NULL) {
      mysql_close(conn->mysql_conn);
      conn->mysql_conn = NULL;
    }

    free(conn->host);
    free(conn->user);
    free(conn->password);
    free(conn->database);
  }

  free(pool->connections);

  pthread_mutex_destroy(&pool->pool_mutex);
  pthread_cond_destroy(&pool->connection_available);

  free(pool);
  LOG_INFO("Connection pool destroyed");
}
