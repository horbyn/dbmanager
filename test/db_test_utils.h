#pragma once

// clang-format off
#include <mysql/mysql.h>
// clang-format on

#define TEST_DB_HOST "localhost"
#define TEST_DB_USER "root"
#define TEST_DB_PASS "root"
#define TEST_DB_NAME "mydb"
#define TEST_TABLE "test_users"

// 测试表结构
#define CREATE_TEST_TABLE_SQL                                                                      \
  "CREATE TABLE IF NOT EXISTS test_users ("                                                        \
  "id INT AUTO_INCREMENT PRIMARY KEY, "                                                            \
  "name VARCHAR(100) NOT NULL, "                                                                   \
  "email VARCHAR(100) NOT NULL, "                                                                  \
  "age INT, "                                                                                      \
  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"                                                 \
  ")"

#define DROP_TEST_TABLE_SQL "DROP TABLE IF EXISTS test_users"

// 测试数据
#define INSERT_TEST_DATA_SQL                                                                       \
  "INSERT INTO test_users (name, email, age) VALUES "                                              \
  "('Alice', 'alice@example.com', 25), "                                                           \
  "('Bob', 'bob@example.com', 30), "                                                               \
  "('Charlie', 'charlie@example.com', 35)"

MYSQL *db_test_connect(void);
void db_test_disconnect(MYSQL *conn);
int db_test_execute(MYSQL *conn, const char *sql);
void db_test_setup_database(void);
void db_test_cleanup_database(void);
int db_test_count_rows(MYSQL *conn, const char *table);
