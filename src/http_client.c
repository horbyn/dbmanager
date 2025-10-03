// clang-format off
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbmanager_conf.h"
#include "http_client.h"
#include "src/key.h"
#include "src/logger.h"
#include "src/macro.h"
// clang-format on

#define VERSION0 DBCLI STR_HELPER(-)
#define VERSION VERSION0 STR_HELPER(OHNO_VERSION)

// HTTP 响应缓冲区
typedef struct {
  char *data;
  size_t size;
} response_buffer_t;

/**
 * @brief libcurl 回调
 *
 * @param contents 数据
 * @param size 数据长度
 * @param nmemb 数据块数量
 * @param userp 用户自定义结构
 * @return size_t 响应总长度
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  response_buffer_t *buffer = (response_buffer_t *)userp;

  char *ptr = realloc(buffer->data, buffer->size + total_size + 1);
  if (!ptr) {
    LOG_ERROR("Failed to allocate memory for HTTP response");
    return 0;
  }

  buffer->data = ptr;
  memcpy(&(buffer->data[buffer->size]), contents, total_size);
  buffer->size += total_size;
  buffer->data[buffer->size] = '\0';

  return total_size;
}

/**
 * @brief URL 编码函数
 *
 * @param str 要编码的字符串
 * @return char* 编码后的字符串
 */
static char *url_encode(const char *str) {
  if (!str) {
    return NULL;
  }

  // 计算需要的缓冲区大小
  size_t len = strlen(str);
  char *encoded = malloc(len * 3 + 1); // 最坏情况：每个字符变成 %XX
  if (!encoded) {
    return NULL;
  }

  char *ptr = encoded;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = str[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      *ptr++ = c;
    } else {
      sprintf(ptr, "%%%02X", c);
      ptr += 3;
    }
  }
  *ptr = '\0';

  return encoded;
}

/**
 * @brief 初始化 http client
 *
 * @param base_url url
 * @return http_client_t* 对象
 */
http_client_t *http_client_init(const char *base_url) {
  http_client_t *client = malloc(sizeof(http_client_t));
  if (!client) {
    LOG_ERROR("Failed to allocate memory for HTTP client");
    return NULL;
  }

  client->curl = curl_easy_init();
  if (!client->curl) {
    LOG_ERROR("Failed to initialize libcurl");
    free(client);
    return NULL;
  }

  client->base_url = strdup(base_url);

  curl_easy_setopt(client->curl, CURLOPT_USERAGENT, VERSION);
  curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(client->curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, 1L);

  LOG_DEBUG("HTTP client initialized with base URL: %s", base_url);
  return client;
}

/**
 * @brief 销毁 http client
 *
 * @param client 对象
 */
void http_client_cleanup(http_client_t *client) {
  if (client) {
    if (client->curl) {
      curl_easy_cleanup(client->curl);
    }
    if (client->base_url) {
      free(client->base_url);
    }
    free(client);
  }
}

/**
 * @brief 发送 http 请求
 *
 * @param client http client 对象
 * @param operation 操作类型
 * @param table 表
 * @param data 数据
 * @param where 条件
 * @param output 输出（仅 READ 操作使用）
 * @return int 出错返回 -1，成功返回值大于等于 0
 */
static int send_http_request(http_client_t *client, const char *operation, const char *table,
                             const char *data, const char *where, char **output) {
  if (!client || !client->curl) {
    return -1;
  }

  char *encoded_operation = url_encode(operation);
  char *encoded_table = url_encode(table);
  char *encoded_data = data ? url_encode(data) : NULL;
  char *encoded_where = where ? url_encode(where) : NULL;

  size_t post_data_len = strlen(encoded_operation) + strlen(encoded_table) + 50;
  if (encoded_data) {
    post_data_len += strlen(encoded_data) + 10;
  }
  if (encoded_where) {
    post_data_len += strlen(encoded_where) + 10;
  }

  char *post_data = malloc(post_data_len);
  if (!post_data) {
    LOG_ERROR("Failed to allocate memory for POST data");
    free(encoded_operation);
    free(encoded_table);
    if (encoded_data) {
      free(encoded_data);
    }
    if (encoded_where) {
      free(encoded_where);
    }
    return -1;
  }

  snprintf(post_data, post_data_len, "%s=%s&%s=%s", KEY_POST_OPERATION, encoded_operation,
           KEY_POST_TABLE, encoded_table);

  if (encoded_data) {
    strcat(post_data, "&");
    strcat(post_data, KEY_POST_DATA);
    strcat(post_data, "=");
    strcat(post_data, encoded_data);
  }

  if (encoded_where) {
    strcat(post_data, "&");
    strcat(post_data, KEY_POST_WHERE);
    strcat(post_data, "=");
    strcat(post_data, encoded_where);
  }

  LOG_DEBUG("Sending HTTP request: %s", post_data);

  // 准备 HTTP 请求
  response_buffer_t response_buffer = {0};
  curl_easy_setopt(client->curl, CURLOPT_URL, client->base_url);
  curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, post_data);
  curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &response_buffer);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(client->curl);
  curl_slist_free_all(headers);

  free(encoded_operation);
  free(encoded_table);
  if (encoded_data) {
    free(encoded_data);
  }
  if (encoded_where) {
    free(encoded_where);
  }
  free(post_data);

  if (res != CURLE_OK) {
    LOG_ERROR("HTTP request failed: %s", curl_easy_strerror(res));
    if (response_buffer.data) {
      free(response_buffer.data);
    }
    return -1;
  }

  LOG_DEBUG("Received HTTP response: %s", response_buffer.data);

  // 解析 HTTP 响应
  int result = -1;
  size_t len_succ = strlen(KEY_RESP_SUCCESS);
  size_t len_fail = strlen(KEY_RESP_ERROR);
  if (strncmp(response_buffer.data, KEY_RESP_SUCCESS, len_succ) == 0) {
    // CREATE, UPDATE, DELETE
    char *ptr = response_buffer.data + 8;
    *output = strdup(ptr);
    while (*ptr && !(*ptr >= '0' && *ptr <= '9')) {
      ptr++;
    }
    if (*ptr) {
      result = atoi(ptr);
    } else {
      result = 0;
    }
  } else if (strncmp(response_buffer.data, KEY_RESP_ERROR, len_fail) == 0) {
    *output = strdup(response_buffer.data + 6);
  } else {
    // READ
    if (strcmp(operation, KEY_OP_READ) == 0 && output) {
      *output = strdup(response_buffer.data);
      result = 1;
    }
  }

  free(response_buffer.data);
  return result;
}

/**
 * @brief 通过 http 发起数据库 create
 *
 * @param client http client
 * @param table 表
 * @param data 数据
 * @param output 返回值
 * @return int 出错（-1）；成功（大于等于 0，含义为已生效的条目数）
 */
int http_client_create(http_client_t *client, const char *table, const char *data, char **output) {
  return send_http_request(client, KEY_OP_CREATE, table, data, NULL, output);
}

/**
 * @brief 通过 http 发起数据库 read
 *
 * @param client http client
 * @param table 表
 * @param where 条件
 * @param output 返回值
 * @return int 出错（-1）；成功（1）
 */
int http_client_read(http_client_t *client, const char *table, const char *where, char **output) {
  return send_http_request(client, KEY_OP_READ, table, NULL, where, output);
}

/**
 * @brief 通过 http 发起数据库 update
 *
 * @param client http client
 * @param table 表
 * @param data 数据
 * @param where 条件
 * @param output 返回值
 * @return int 出错（-1）；成功（大于等于 0，含义为已生效的条目数）
 */
int http_client_update(http_client_t *client, const char *table, const char *data,
                       const char *where, char **output) {
  return send_http_request(client, KEY_OP_UPDATE, table, data, where, output);
}

/**
 * @brief 通过 http 发起数据库 delete
 *
 * @param client http client
 * @param table 表
 * @param where 条件
 * @param output 返回值
 * @return int 出错（-1）；成功（大于等于 0，含义为已生效的条目数）
 */
int http_client_delete(http_client_t *client, const char *table, const char *where, char **output) {
  return send_http_request(client, KEY_OP_DELETE, table, NULL, where, output);
}
