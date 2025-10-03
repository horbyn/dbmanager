// clang-format off
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "http_server.h"
#include "src/assert.h"
#include "src/key.h"
#include "src/logger.h"
// clang-format on

// 连接上下文结构
typedef struct connection_info {
  struct MHD_PostProcessor *pp;
  char *operation;
  char *table;
  char *data;
  char *where;
} connection_info_t;

static http_server_t *global_server = NULL;

/**
 * @brief 信号处理函数
 *
 * @param sig 信号
 */
static void signal_handler(int sig) {
  if (global_server) {
    LOG_INFO("Received signal %d, shutting down HTTP server...", sig);
    http_server_stop(global_server);
  }
}

/**
 * @brief 释放连接上下文
 *
 * @param cls 连接上下文
 */
static void free_connection_info(void *cls) {
  connection_info_t *con_info = (connection_info_t *)cls;
  if (con_info) {
    if (con_info->pp) {
      MHD_destroy_post_processor(con_info->pp);
    }
    if (con_info->operation) {
      free(con_info->operation);
    }
    if (con_info->table) {
      free(con_info->table);
    }
    if (con_info->data) {
      free(con_info->data);
    }
    if (con_info->where) {
      free(con_info->where);
    }
    free(con_info);
  }
}

/**
 * @brief POST 数据处理迭代器
 *
 * @param cls 连接上下文
 * @param kind 值类型
 * @param key 字段名
 * @param filename 文件名（用于文件上传）
 * @param content_type 内容类型
 * @param transfer_encoding 传输编码
 * @param data 数据
 * @param off 偏移量
 * @param size 数据大小
 * @return enum MHD_Result MHD_YES 或 MHD_NO
 */
static enum MHD_Result post_data_iterator(void *cls, enum MHD_ValueKind kind, const char *key,
                                          const char *filename, const char *content_type,
                                          const char *transfer_encoding, const char *data,
                                          uint64_t off, size_t size) {
  (void)kind;
  (void)filename;
  (void)content_type;
  (void)transfer_encoding;
  (void)off;
  connection_info_t *con_info = (connection_info_t *)cls;

  if (key == NULL || data == NULL || size == 0) {
    return MHD_YES;
  }

  char **target_field = NULL;
  if (strcmp(key, KEY_POST_OPERATION) == 0) {
    target_field = &con_info->operation;
  } else if (strcmp(key, KEY_POST_TABLE) == 0) {
    target_field = &con_info->table;
  } else if (strcmp(key, KEY_POST_DATA) == 0) {
    target_field = &con_info->data;
  } else if (strcmp(key, KEY_POST_WHERE) == 0) {
    target_field = &con_info->where;
  }

  if (target_field != NULL) {
    if (*target_field) {
      free(*target_field);
    }

    *target_field = malloc(size + 1);
    if (*target_field) {
      memcpy(*target_field, data, size);
      (*target_field)[size] = '\0';
      LOG_DEBUG("Post key %s -> %s", key, *target_field);
    } else {
      LOG_ERROR("Failed to allocate memory for field: %s", key);
      return MHD_NO;
    }
  } else {
    LOG_WARN("Unknown POST field: %s", key);
  }

  return MHD_YES;
}

/**
 * @brief 将数据库结果集序列化为简单的文本格式
 *
 * @param result 数据库结果集
 * @return char* 格式化文本
 */
static char *serialize_db_result(db_result_t *result) {
  if (!result || !result->mysql_res) {
    return strdup("No sql results\n");
  }

  // 计算需要的缓冲区大小
  size_t buffer_size = 1024;
  char *buffer = malloc(buffer_size);
  if (!buffer) {
    return NULL;
  }

  MYSQL_ROW row;
  MYSQL_FIELD *fields = mysql_fetch_fields(result->mysql_res);
  int offset = 0;

  // 添加表头
  for (int i = 0; i < result->num_fields && (size_t)offset < buffer_size - 1; i++) {
    offset += snprintf(buffer + offset, buffer_size - offset, "%-15s", fields[i].name);
  }
  offset += snprintf(buffer + offset, buffer_size - offset, "\n");

  // 添加分隔线
  for (int i = 0; i < result->num_fields && (size_t)offset < buffer_size - 1; i++) {
    offset += snprintf(buffer + offset, buffer_size - offset, "%-15s", "---------------");
  }
  offset += snprintf(buffer + offset, buffer_size - offset, "\n");

  // 添加数据行
  while ((row = mysql_fetch_row(result->mysql_res)) && (size_t)offset < buffer_size - 1) {
    for (int i = 0; i < result->num_fields && (size_t)offset < buffer_size - 1; i++) {
      offset += snprintf(buffer + offset, buffer_size - offset, "%-15s", row[i] ? row[i] : "NULL");
    }
    offset += snprintf(buffer + offset, buffer_size - offset, "\n");
  }

  return buffer;
}

/**
 * @brief 处理数据库请求
 *
 * @param db_mgr 数据库管理对象
 * @param con_info 连接上下文
 * @return char* 响应字符串
 */
static char *handle_db_request(db_manager_t *db_mgr, connection_info_t *con_info) {
  if (!con_info->operation || !con_info->table) {
    return strdup(KEY_RESP_ERROR " Missing required fields: operation, table");
  }

  const char *op_str = con_info->operation;
  const char *table_str = con_info->table;
  const char *data_str = con_info->data;
  const char *where_str = con_info->where;

  LOG_INFO("Processing DB operation: %s on table %s", op_str, table_str);

  char *response = NULL;
  char buffer[256];

  if (strcmp(op_str, KEY_OP_CREATE) == 0) {
    if (!data_str) {
      response = strdup(KEY_RESP_ERROR " Missing data field for create operation");
    } else {
      int result = db_manager_create_row(db_mgr, table_str, data_str);
      if (result >= 0) {
        snprintf(buffer, sizeof(buffer), "%s Created %d row(s)", KEY_RESP_SUCCESS, result);
        response = strdup(buffer);
      } else {
        if (db_mgr->last_error != NULL) {
          snprintf(buffer, sizeof(buffer), "%s Create operation failed: %s", KEY_RESP_ERROR,
                   db_mgr->last_error);
          response = strdup(buffer);
        } else {
          response = strdup(KEY_RESP_ERROR " Create operation failed");
        }
      }
    }
  } else if (strcmp(op_str, KEY_OP_READ) == 0) {
    db_result_t *db_result = db_manager_read_row(db_mgr, table_str, where_str);
    if (db_result) {
      response = serialize_db_result(db_result);
      db_result_free(db_result);
    } else {
      if (db_mgr->last_error != NULL) {
        snprintf(buffer, sizeof(buffer), "%s Read operation failed: %s", KEY_RESP_ERROR,
                 db_mgr->last_error);
        response = strdup(buffer);
      } else {
        response = strdup(KEY_RESP_ERROR " Read operation failed");
      }
    }
  } else if (strcmp(op_str, KEY_OP_UPDATE) == 0) {
    if (!data_str || !where_str) {
      response = strdup(KEY_RESP_ERROR " Missing data or where field for update operation");
    } else {
      int result = db_manager_update_row(db_mgr, table_str, data_str, where_str);
      if (result >= 0) {
        snprintf(buffer, sizeof(buffer), "%s Updated %d row(s)", KEY_RESP_SUCCESS, result);
        response = strdup(buffer);
      } else {
        if (db_mgr->last_error != NULL) {
          snprintf(buffer, sizeof(buffer), "%s Update operation failed: %s", KEY_RESP_ERROR,
                   db_mgr->last_error);
          response = strdup(buffer);
        } else {
          response = strdup(KEY_RESP_ERROR " Update operation failed");
        }
      }
    }
  } else if (strcmp(op_str, KEY_OP_DELETE) == 0) {
    if (!where_str) {
      response = strdup(KEY_RESP_ERROR " Missing where field for delete operation");
    } else {
      int result = db_manager_delete_row(db_mgr, table_str, where_str);
      if (result >= 0) {
        snprintf(buffer, sizeof(buffer), "%s Deleted %d row(s)", KEY_RESP_SUCCESS, result);
        response = strdup(buffer);
      } else {
        if (db_mgr->last_error != NULL) {
          snprintf(buffer, sizeof(buffer), "%s Delete operation failed: %s", KEY_RESP_ERROR,
                   db_mgr->last_error);
          response = strdup(buffer);
        } else {
          response = strdup(KEY_RESP_ERROR " Delete operation failed");
        }
      }
    }
  } else {
    response = strdup(KEY_RESP_ERROR " Unknown operation");
  }

  return response;
}

/**
 * @brief HTTP 请求处理回调
 *
 * @param cls 服务器实例
 * @param connection microhttpd 连接的 session
 * @param url 资源地址
 * @param method 方法
 * @param version 版本
 * @param upload_data 上传的数据
 * @param upload_data_size 数据长度
 * @param con_cls 用户自定义数据
 * @return enum MHD_Result 返回值
 */
static enum MHD_Result request_handler(void *cls, struct MHD_Connection *connection,
                                       const char *url, const char *method, const char *version,
                                       const char *upload_data, size_t *upload_data_size,
                                       void **con_cls) {
  DBMNGR_ASSERT(url);
  DBMNGR_ASSERT(method);
  DBMNGR_ASSERT(version);

  LOG_DEBUG("Received request: %s %s %s", method, url, version);
  http_server_t *server = (http_server_t *)cls;

  // 只支持 POST 方法
  if (strcmp(method, "POST") != 0) {
    const char *error_msg = "Only POST method is supported";
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(error_msg), (void *)error_msg, MHD_RESPMEM_PERSISTENT);
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);
    MHD_destroy_response(response);
    return ret;
  }

  connection_info_t *con_info = *con_cls;

  // 第一次调用，初始化连接上下文
  if (con_info == NULL) {
    con_info = calloc(1, sizeof(connection_info_t));
    if (!con_info) {
      LOG_ERROR("Failed to allocate connection info");
      return MHD_NO;
    }

    con_info->operation = NULL;
    con_info->table = NULL;
    con_info->data = NULL;
    con_info->where = NULL;
    con_info->pp = MHD_create_post_processor(connection, 8192, post_data_iterator, con_info);
    if (!con_info->pp) {
      LOG_ERROR("Failed to create post processor");
      free_connection_info(con_info);
      return MHD_NO;
    }

    *con_cls = con_info;
    return MHD_YES;
  }

  // 处理 POST 数据块
  if (*upload_data_size > 0) {
    LOG_DEBUG("Processing POST data chunk, size: %zu", *upload_data_size);

    int ret = MHD_post_process(con_info->pp, upload_data, *upload_data_size);
    if (ret == MHD_NO) {
      LOG_ERROR("Failed to process POST data");
      return MHD_NO;
    }

    *upload_data_size = 0; // 标记数据已处理
    return MHD_YES;
  }

  // POST 数据处理完成（upload_data_size == 0）
  LOG_DEBUG("POST data processing completed");

  // 处理数据库请求
  char *response_str = handle_db_request(server->db_mgr, con_info);
  if (!response_str) {
    response_str = strdup(KEY_RESP_ERROR " Failed to process request");
  }

  LOG_DEBUG("Construct HTTP response:\n%s", response_str);

  // 创建 HTTP 响应
  struct MHD_Response *response = MHD_create_response_from_buffer(
      strlen(response_str), (void *)response_str, MHD_RESPMEM_MUST_FREE);

  if (!response) {
    LOG_ERROR("Failed to create response");
    free(response_str);
    return MHD_NO;
  }

  MHD_add_response_header(response, "Content-Type", "text/plain");

  enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

/**
 * @brief 初始化 http server
 *
 * @param db_mgr 数据库管理对象
 * @return http_server_t* http server 对象
 */
http_server_t *http_server_init(db_manager_t *db_mgr) {
  http_server_t *server = malloc(sizeof(http_server_t));
  if (!server) {
    LOG_ERROR("Failed to allocate memory for HTTP server");
    return NULL;
  }

  server->db_mgr = db_mgr;
  server->running = false;
  server->daemon = NULL;

  global_server = server;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  return server;
}

/**
 * @brief 启动 http 服务
 *
 * @param server http server 对象
 * @return int 成功（0）；失败（-1）
 */
int http_server_start(http_server_t *server) {
  if (!server)
    return -1;

  server->daemon = MHD_start_daemon(
      MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_DEBUG, HTTP_PORT, NULL, NULL, &request_handler,
      server, MHD_OPTION_NOTIFY_COMPLETED, &free_connection_info, NULL, MHD_OPTION_END);

  if (!server->daemon) {
    LOG_ERROR("Failed to start HTTP server on port %d", HTTP_PORT);
    return -1;
  }

  server->running = true;
  LOG_INFO("HTTP server started on port %d", HTTP_PORT);
  return 0;
}

/**
 * @brief 停止 http 服务端
 *
 * @param server http server 对象
 */
void http_server_stop(http_server_t *server) {
  if (server && server->running) {
    MHD_stop_daemon(server->daemon);
    server->running = false;
    LOG_INFO("HTTP server stopped");
  }
}

/**
 * @brief 销毁 http server
 *
 * @param server http server 对象
 */
void http_server_destroy(http_server_t *server) {
  if (!server)
    return;

  http_server_stop(server);
  free(server);
  global_server = NULL;
}
