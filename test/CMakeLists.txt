add_library(utils
  STATIC
  db_test_utils.c
)

add_executable(test_connection_pool test_connection_pool.c)
target_link_libraries(test_connection_pool
  PRIVATE
  unity::framework
  ${PROJECT_NAME}::core
)
add_test(test_connection_pool test_connection_pool)

add_executable(test_db_manager test_db_manager.c)
target_link_libraries(test_db_manager
  PRIVATE
  utils
  unity::framework
  ${PROJECT_NAME}::core
)
add_test(test_db_manager test_db_manager)
