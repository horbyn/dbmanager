// clang-format off
#include <time.h>
#include <unistd.h>
#include "unity.h"
#include "db_test_utils.h"
#include "src/connection_pool.h"
// clang-format on

static const size_t MAX_POOL_SIZE = 3;
static connection_pool_t *test_pool = NULL;

void setUp(void) {
  test_pool =
      create_connection_pool(TEST_DB_HOST, TEST_DB_USER, TEST_DB_PASS, TEST_DB_NAME, MAX_POOL_SIZE);
}

void tearDown(void) {
  if (test_pool) {
    destroy_connection_pool(test_pool);
    test_pool = NULL;
  }
}

void test_create_connection_pool_success(void) {
  TEST_ASSERT_NOT_NULL(test_pool);
  TEST_ASSERT_EQUAL_INT(MAX_POOL_SIZE, test_pool->pool_size);
  TEST_ASSERT_EQUAL_INT(0, test_pool->active_connections);
  TEST_ASSERT_FALSE(test_pool->shutdown);
}

void test_get_and_release_connection(void) {
  TEST_ASSERT_NOT_NULL(test_pool);

  // 获取连接
  mysql_connection_t *conn = get_connection(test_pool);
  TEST_ASSERT_NOT_NULL(conn);
  TEST_ASSERT_TRUE(conn->in_use);
  TEST_ASSERT_EQUAL_INT(1, test_pool->active_connections);

  // 释放连接
  release_connection(test_pool, conn);
  TEST_ASSERT_FALSE(conn->in_use);
  TEST_ASSERT_EQUAL_INT(0, test_pool->active_connections);
}

void test_get_connection_from_shutdown_pool(void) {
  TEST_ASSERT_NOT_NULL(test_pool);

  // 关闭连接池
  test_pool->shutdown = true;

  // 尝试从已关闭的连接池获取连接
  mysql_connection_t *conn = get_connection(test_pool);
  TEST_ASSERT_NULL(conn);
}

void test_multiple_connections(void) {
  TEST_ASSERT_NOT_NULL(test_pool);

  // 获取多个连接
  mysql_connection_t *conn1 = get_connection(test_pool);
  mysql_connection_t *conn2 = get_connection(test_pool);
  mysql_connection_t *conn3 = get_connection(test_pool);

  TEST_ASSERT_NOT_NULL(conn1);
  TEST_ASSERT_NOT_NULL(conn2);
  TEST_ASSERT_NOT_NULL(conn3);

  TEST_ASSERT_EQUAL_INT(3, test_pool->active_connections);

  // 释放连接
  release_connection(test_pool, conn1);
  release_connection(test_pool, conn2);
  release_connection(test_pool, conn3);

  TEST_ASSERT_EQUAL_INT(0, test_pool->active_connections);
}

void test_check_connection_health(void) {
  TEST_ASSERT_NOT_NULL(test_pool);

  mysql_connection_t *conn = get_connection(test_pool);
  TEST_ASSERT_NOT_NULL(conn);

  bool healthy = check_connection_health(conn);
  TEST_ASSERT_TRUE(healthy);

  release_connection(test_pool, conn);
}

void test_destroy_connection_pool(void) {
  TEST_ASSERT_NOT_NULL(test_pool);

  mysql_connection_t *conn1 = get_connection(test_pool);
  mysql_connection_t *conn2 = get_connection(test_pool);

  TEST_ASSERT_NOT_NULL(conn1);
  TEST_ASSERT_NOT_NULL(conn2);

  destroy_connection_pool(test_pool);
  test_pool = NULL;
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_create_connection_pool_success);
  RUN_TEST(test_get_and_release_connection);
  RUN_TEST(test_get_connection_from_shutdown_pool);
  RUN_TEST(test_multiple_connections);
  RUN_TEST(test_check_connection_health);
  RUN_TEST(test_destroy_connection_pool);

  return UNITY_END();
}
