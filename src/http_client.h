#pragma once

// clang-format off
#include "curl/curl.h"
// clang-format on

typedef struct {
  CURL *curl;
  char *base_url;
} http_client_t;

http_client_t *http_client_init(const char *base_url);
void http_client_cleanup(http_client_t *client);
int http_client_create(http_client_t *client, const char *table, const char *data, char **output);
int http_client_read(http_client_t *client, const char *table, const char *where, char **output);
int http_client_update(http_client_t *client, const char *table, const char *data,
                       const char *where, char **output);
int http_client_delete(http_client_t *client, const char *table, const char *where, char **output);
