// clang-format off
#include "unity.h"
#include "db_test_utils.h"
#include "src/db_manager.h"
// clang-format on

static const size_t MAX_POOL_SIZE = 3;
static db_manager_t *test_manager = NULL;

void setUp(void) {
  db_test_setup_database();
  test_manager =
      db_manager_init(TEST_DB_HOST, TEST_DB_USER, TEST_DB_PASS, TEST_DB_NAME, MAX_POOL_SIZE);
}

void tearDown(void) {
  if (test_manager) {
    db_manager_destroy(test_manager);
    test_manager = NULL;
  }
  db_test_cleanup_database();
}

void test_db_manager_init_success(void) {
  TEST_ASSERT_NOT_NULL(test_manager);
  TEST_ASSERT_NOT_NULL(test_manager->conn_pool);
  TEST_ASSERT_EQUAL_INT(DB_MAX_RETRIES, test_manager->max_retries);
  TEST_ASSERT_NULL(test_manager->last_error);
}

void test_db_manager_create_row_success(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 插入新记录
  int result = db_manager_create_row(test_manager, TEST_TABLE,
                                     "name='David', email='david@example.com', age=28");
  TEST_ASSERT_EQUAL_INT(1, result);

  // 验证记录已插入
  MYSQL *conn = db_test_connect();
  int count = db_test_count_rows(conn, TEST_TABLE);
  db_test_disconnect(conn);

  TEST_ASSERT_EQUAL_INT(4, count); // 原有3条 + 新插入1条
}

void test_db_manager_create_row_invalid_params(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 测试空表名
  int result = db_manager_create_row(test_manager, NULL, "name='test'");
  TEST_ASSERT_EQUAL_INT(-1, result);

  // 测试空数据
  result = db_manager_create_row(test_manager, TEST_TABLE, NULL);
  TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_db_manager_read_row_success(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 读取所有记录
  db_result_t *result = db_manager_read_row(test_manager, TEST_TABLE, NULL);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_INT(3, result->num_rows); // 初始有3条记录

  if (result) {
    db_result_free(result);
  }

  // 带条件读取
  result = db_manager_read_row(test_manager, TEST_TABLE, "name='Alice'");
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_INT(1, result->num_rows); // 只有1条记录满足条件

  if (result) {
    db_result_free(result);
  }
}

void test_db_manager_read_row_invalid_params(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 测试空表名
  db_result_t *result = db_manager_read_row(test_manager, NULL, "name='Alice'");
  TEST_ASSERT_NULL(result);
}

void test_db_manager_update_row_success(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 更新记录
  int result = db_manager_update_row(test_manager, TEST_TABLE, "age=26", "name='Alice'");
  TEST_ASSERT_EQUAL_INT(1, result);

  // 验证更新
  db_result_t *read_result =
      db_manager_read_row(test_manager, TEST_TABLE, "name='Alice' AND age=26");
  TEST_ASSERT_NOT_NULL(read_result);
  TEST_ASSERT_EQUAL_INT(1, read_result->num_rows);

  if (read_result) {
    db_result_free(read_result);
  }
}

void test_db_manager_update_row_invalid_params(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 测试空表名
  int result = db_manager_update_row(test_manager, NULL, "age=26", "name='Alice'");
  TEST_ASSERT_EQUAL_INT(-1, result);

  // 测试空数据
  result = db_manager_update_row(test_manager, TEST_TABLE, NULL, "name='Alice'");
  TEST_ASSERT_EQUAL_INT(-1, result);

  // 测试空条件
  result = db_manager_update_row(test_manager, TEST_TABLE, "age=26", NULL);
  TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_db_manager_delete_row_success(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 删除记录
  int result = db_manager_delete_row(test_manager, TEST_TABLE, "name='Alice'");
  TEST_ASSERT_EQUAL_INT(1, result);

  // 验证删除
  MYSQL *conn = db_test_connect();
  int count = db_test_count_rows(conn, TEST_TABLE);
  db_test_disconnect(conn);

  TEST_ASSERT_EQUAL_INT(2, count); // 原有3条 - 删除1条
}

void test_db_manager_delete_row_invalid_params(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 测试空表名
  int result = db_manager_delete_row(test_manager, NULL, "name='Alice'");
  TEST_ASSERT_EQUAL_INT(-1, result);

  // 测试空条件
  result = db_manager_delete_row(test_manager, TEST_TABLE, NULL);
  TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_db_manager_error_handling(void) {
  TEST_ASSERT_NOT_NULL(test_manager);

  // 执行错误的SQL语句
  int result = db_manager_create_row(test_manager, TEST_TABLE, "invalid_sql_syntax");
  TEST_ASSERT_EQUAL_INT(-1, result);

  // 检查错误信息是否被设置
  TEST_ASSERT_NOT_NULL(test_manager->last_error);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_db_manager_init_success);
  RUN_TEST(test_db_manager_create_row_success);
  RUN_TEST(test_db_manager_create_row_invalid_params);
  RUN_TEST(test_db_manager_read_row_success);
  RUN_TEST(test_db_manager_read_row_invalid_params);
  RUN_TEST(test_db_manager_update_row_success);
  RUN_TEST(test_db_manager_update_row_invalid_params);
  RUN_TEST(test_db_manager_delete_row_success);
  RUN_TEST(test_db_manager_delete_row_invalid_params);
  RUN_TEST(test_db_manager_error_handling);

  return UNITY_END();
}
