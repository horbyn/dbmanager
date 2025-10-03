#pragma once

// clang-format off
#include <microhttpd.h>
#include "src/db_manager.h"
#include "src/macro.h"
// clang-format on

typedef struct {
  struct MHD_Daemon *daemon;
  db_manager_t *db_mgr;
  bool running;
} http_server_t;

http_server_t *http_server_init(db_manager_t *db_mgr);
int http_server_start(http_server_t *server);
void http_server_stop(http_server_t *server);
void http_server_destroy(http_server_t *server);
