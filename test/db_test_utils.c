// clang-format off
#include <stdio.h>
#include <stdlib.h>
#include "db_test_utils.h"
// clang-format on

MYSQL *db_test_connect(void) {
  MYSQL *conn = mysql_init(NULL);
  if (!conn) {
    fprintf(stderr, "mysql_init() failed\n");
    return NULL;
  }

  if (!mysql_real_connect(conn, TEST_DB_HOST, TEST_DB_USER, TEST_DB_PASS, TEST_DB_NAME, 0, NULL,
                          0)) {
    fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
    mysql_close(conn);
    return NULL;
  }

  return conn;
}

void db_test_disconnect(MYSQL *conn) {
  if (conn) {
    mysql_close(conn);
  }
}

int db_test_execute(MYSQL *conn, const char *sql) {
  if (mysql_query(conn, sql) != 0) {
    fprintf(stderr, "Query failed: %s\n", mysql_error(conn));
    return -1;
  }
  return 0;
}

void db_test_setup_database(void) {
  MYSQL *conn = db_test_connect();
  if (!conn) {
    fprintf(stderr, "Failed to connect to test database\n");
    return;
  }

  // 创建测试表
  if (db_test_execute(conn, DROP_TEST_TABLE_SQL) != 0) {
    fprintf(stderr, "Failed to drop test table\n");
  }

  if (db_test_execute(conn, CREATE_TEST_TABLE_SQL) != 0) {
    fprintf(stderr, "Failed to create test table\n");
  }

  // 插入测试数据
  if (db_test_execute(conn, INSERT_TEST_DATA_SQL) != 0) {
    fprintf(stderr, "Failed to insert test data\n");
  }

  db_test_disconnect(conn);
}

void db_test_cleanup_database(void) {
  MYSQL *conn = db_test_connect();
  if (!conn) {
    return;
  }

  // 清理测试表
  db_test_execute(conn, DROP_TEST_TABLE_SQL);

  db_test_disconnect(conn);
}

int db_test_count_rows(MYSQL *conn, const char *table) {
  char query[256];
  snprintf(query, sizeof(query), "SELECT COUNT(*) FROM %s", table);

  if (mysql_query(conn, query) != 0) {
    return -1;
  }

  MYSQL_RES *result = mysql_store_result(conn);
  if (!result) {
    return -1;
  }

  MYSQL_ROW row = mysql_fetch_row(result);
  int count = row ? atoi(row[0]) : 0;

  mysql_free_result(result);
  return count;
}
