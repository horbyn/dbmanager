cmake_minimum_required(VERSION 3.17)
project("dbmanager"
  VERSION 0.1.0
  LANGUAGES C CXX
)
message(STATUS "[版本]: ${PROJECT_NAME}_v${PROJECT_VERSION}")

option(DBMNGR_ENABLE_STATIC_LIB "第三方库使用静态链接" ON)
option(DBMNGR_ENABLE_TEST "单元测试" OFF)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  message(STATUS "[编译选项]: debug")
  add_compile_options(-Wall -Wextra -O0 -g3 -D_GLIBCXX_DEBUG)
else()
  message(STATUS "[编译选项]: release")
  add_compile_options(-Wall -Wextra -O2 -DNDEBUG)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(DBMNGR_ENABLE_STATIC_LIB)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

set(DBMNGR_DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)
set(DBMNGR_DEPS_BUILD_DIR ${CMAKE_BINARY_DIR}/deps)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include(FetchContent)

FetchContent_Declare(
  zlog
  GIT_REPOSITORY https://github.com/HardySimpson/zlog.git
  GIT_TAG 1.2.18
  SOURCE_DIR   "${DBMNGR_DEPS_DIR}/zlog_src"
  BINARY_DIR   "${DBMNGR_DEPS_BUILD_DIR}/zlog"
  SUBBUILD_DIR "${DBMNGR_DEPS_DIR}/zlog_subbuild"
)
FetchContent_MakeAvailable(zlog)
message(STATUS "zlog 头文件目录: ${zlog_SOURCE_DIR}/src")
include_directories(${zlog_SOURCE_DIR}/src)
set(LOG_CONF /etc/dbmanager.conf)
file(COPY_FILE ${CMAKE_SOURCE_DIR}/conf/dbmanager.conf ${LOG_CONF})
message(STATUS "zlog 日志配置已拷贝至 ${LOG_CONF}")

FetchContent_Declare(
  curl
  GIT_REPOSITORY https://github.com/curl/curl.git
  GIT_TAG curl-8_12_0
  SOURCE_DIR   "${DBMNGR_DEPS_DIR}/curl_src"
  BINARY_DIR   "${DBMNGR_DEPS_BUILD_DIR}/curl"
  SUBBUILD_DIR "${DBMNGR_DEPS_DIR}/curl_subbuild"
)
set(BUILD_CURL_EXE OFF CACHE BOOL "构建 curl 命令行工具" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "构建 libcurl example" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "构建 libcurl 文档" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "构建 misc 文档如 curl-config 或 mk-ca-bundle" FORCE)
if(DBMNGR_ENABLE_STATIC_LIB)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "构建 libcurl 动态库" FORCE)
  set(BUILD_STATIC_LIBS ON CACHE BOOL "构建 libcurl 静态库" FORCE)
else()
  message(FATAL_ERROR "暂不支持动态链接")
endif()
set(BUILD_TESTING ON CACHE BOOL "构建 libcurl 单元测试" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "构建文档并启用它的 -M/--manual 选项" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "不安装 libcurl" FORCE)
FetchContent_MakeAvailable(curl)
set(CURL_INCLUDE_DIR ${curl_SOURCE_DIR}/include)
message(STATUS "curl 头文件目录: ${CURL_INCLUDE_DIR}")
include_directories(${CURL_INCLUDE_DIR})
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  set(CURL_LIBRARY ${curl_BINARY_DIR}/lib/libcurl-d.a)
else()
  set(CURL_LIBRARY ${curl_BINARY_DIR}/lib/libcurl.a)
endif()
message(STATUS "curl 库文件目录: ${curl_BINARY_DIR}/lib")

if(DBMNGR_ENABLE_TEST)
  FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.1
    SOURCE_DIR   "${DBMNGR_DEPS_DIR}/unity_src"
    BINARY_DIR   "${DBMNGR_DEPS_BUILD_DIR}/unity"
    SUBBUILD_DIR "${DBMNGR_DEPS_DIR}/unity_subbuild"
  )
  FetchContent_MakeAvailable(unity)
  message(STATUS "unity 头文件目录: ${unity_SOURCE_DIR}/src")
  include_directories(${unity_SOURCE_DIR}/src)
endif()

set(DBCLI dbcli)
configure_file(
  ${CMAKE_SOURCE_DIR}/dbmanager_conf.h.in
  ${CMAKE_BINARY_DIR}/dbmanager_conf.h
  @ONLY
)

add_compile_options(-Werror) # 仅作用于当前项目，不影响第三方库
include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
)

add_subdirectory(src)
if(DBMNGR_ENABLE_TEST)
  include(CTest)
  add_subdirectory(test)
endif()

add_library(${PROJECT_NAME}_core INTERFACE)
target_link_libraries(${PROJECT_NAME}_core INTERFACE
  src
)
add_library(${PROJECT_NAME}::core ALIAS ${PROJECT_NAME}_core)

add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME}
  PRIVATE
  ${PROJECT_NAME}::core
)
set_target_properties(${PROJECT_NAME} PROPERTIES
  INSTALL_RPATH "/opt/dbmngr/libs" # 打包、安装的时候将动态库安装到 rpath 指定的目录
)
add_executable(${DBCLI} dbcli.c)
target_link_libraries(${DBCLI}
  PRIVATE
  ${PROJECT_NAME}::core
)
