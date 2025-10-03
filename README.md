# dbmanager

## Overview

This project is a MySQL management tool using C/S architecture, which consists of two parts: a daemon (`dbmanager`) and a CLI (`dbcli`). These two processes communicate with each other by HTTP. The daemon is a server which listens on port `60001` defaultly to manage MySQL connection pool and execute CRUD operations. While the CLI is the client to parse command line arguments and send HTTP POST requests to the daemon.

## Dependencies

### Baremetal

```shell
# Ubuntu 22.04+
apt-get install -y mysql-server libmysqlclient-dev libmicrohttpd-dev libffi-dev libunistring-dev libpsl-dev libnghttp2-dev
```

### Docker

Create Docker image:

```shell
# ARM 64-bit
docker buildx build --load --platform linux/arm64 --build-arg ARCH=aarch64 -t dbmanager:latest deploy/Dockerfile
# x86_64
docker buildx build --load --platform linux/amd64 --build-arg ARCH=x86_64 -t dbmanager:latest deploy/Dockerfile
```

Create Docker container:

```shell
docker run -d --privileged --name dbmanager -v /host/path/to/dbmanager/:/dbmanager -p 60001:60001 -w /dbmanager dbmanager:latest /sbin/init
```

Enter Docker container:

```shell
docker exec -it dbmanager /sbin/bash
```

## Build

```shell
# debug edition
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDBMNGR_ENABLE_TEST=ON
cmake --build build -j $(getconf _NPROCESSORS_ONLN)

# release edition
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release
cmake --build release -j $(getconf _NPROCESSORS_ONLN)
```

## Packaging and deployment

Run the pack script ([scripts/pack.sh](scripts/pack.sh) in details) which will create a tarball in `packages` directory.

The named format in `package` directory is `dbmngr.{ARCH}.tar.gz`, for example `dbmngr.x86_64.tar.gz` in x86 or `dbmngr.aarch64.tar.gz` in arm.

```shell
$ scripts/pack.sh                                                     
'/dbmanager/release/dbmanager' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/dbmanager'
'/dbmanager/release/dbcli' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/dbcli'
'/dbmanager/conf/dbmanager.conf' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/conf/dbmanager.conf'
'/dbmanager/conf/dbmanager.service' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/conf/dbmanager.service'
'/lib/aarch64-linux-gnu/libp11-kit.so.0' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/libs/libp11-kit.so.0'
'/lib/aarch64-linux-gnu/libstdc++.so.6' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/libs/libstdc++.so.6'
'/lib/aarch64-linux-gnu/libffi.so.8' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/libs/libffi.so.8'
'/dbmanager/scripts/install.sh' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/scripts/install.sh'
'/dbmanager/scripts/uninstall.sh' -> '/tmp/dbmngr_pack_ggZpCo/dbmngr/scripts/uninstall.sh'
✅ 打包完成：/dbmanager/packages/dbmngr.aarch64.tar.gz
包含文件：
./
./dbmngr/
./dbmngr/dbcli
./dbmngr/conf/
./dbmngr/conf/dbmanager.service
./dbmngr/conf/dbmanager.conf
./dbmngr/libs/
./dbmngr/libs/libstdc++.so.6
./dbmngr/libs/libp11-kit.so.0
./dbmngr/libs/libffi.so.8
./dbmngr/dbmanager
./dbmngr/scripts/
./dbmngr/scripts/install.sh
./dbmngr/scripts/uninstall.sh
😯 清理临时目录 /tmp/dbmngr_pack_ggZpCo
```

Copy the package to the target machine:

```shell
scp packages/dbmngr.aarch64.tar.gz root@target:/path/to/store
```

Unpack and install it:

```shell
tar -zxvf dbmngr.aarch64.tar.gz
cd dbmngr
scripts/install.sh
```

*WARNING: Only Ubuntu 22.04+ tested*

## Usage

Suppose we have executed the following commands to create a database and a table:

```shell
mysql> create database mydb;
Query OK, 1 row affected (0.01 sec)

mysql> show databases;
+--------------------+
| Database           |
+--------------------+
| information_schema |
| mydb               |
| mysql              |
| performance_schema |
| sys                |
+--------------------+
5 rows in set (0.00 sec)

mysql> use mydb;
Database changed

mysql> create table users(id int auto_increment primary key,name varchar(100),age int);
Query OK, 0 rows affected (0.05 sec)
```

Then we could execute the following commands to manage the database.

### Create

We could use curl or dbcli to issue CRUD operations:

```shell
# http url decode format does not work with: "name='Alice',age=30".
# it needs changing = to %3D
#          changing , to %2C
#          changing ' to %27
curl -X POST http://localhost:60001 -H "Content-Type: application/x-www-form-urlencoded" -d "operation=create&table=users&data=name%3D%27Alice%27%2Cage%3D30"
./dbcli create --table=users --data="name='Alice',age=30"
```

### Read

```shell
curl -X POST http://localhost:60001 -H "Content-Type: application/x-www-form-urlencoded" -d "operation=read&table=users&where=id%3D1"
./dbcli read --table=users --where="id=1"
```

### Update

```shell
curl -X POST http://localhost:60001 -H "Content-Type: application/x-www-form-urlencoded" -d "operation=update&table=users&data=age%3D31&where=id%3D1"
./dbcli update --table=users --data="age=31" --where="id=1"
```

### Delete

```shell
curl -X POST http://localhost:60001 -H "Content-Type: application/x-www-form-urlencoded" -d "operation=delete&table=users&where=id%3D1"
./dbcli delete --table=users --where="id=1"
```

## Architecture

```shell
┌─────────────────┐    HTTP POST    ┌──────────────────┐
│   CLI Client    │ ───────────────▶│   HTTP Server    │
│     (dbcli)     │                 │     (daemon)     │
└─────────────────┘                 └──────────────────┘
                                              │
                                              ▼
                                    ┌──────────────────┐
                                    │    DB Manager    │
                                    │ (create/read/..) │
                                    └──────────────────┘
                                              │
                                              ▼
                                    ┌──────────────────┐
                                    │ Connection Pool  │
                                    └──────────────────┘
                                              │
                                              ▼
                                    ┌──────────────────┐
                                    │  MySQL Database  │
                                    └──────────────────┘
```

The main principle of this project like above is to separate the business logic from the database operations:

- Daemon Process:
  - The lowest layer: the connection pool module is responsible for managing connections to the MySQL database, including the creation, health checks, reuse, and destruction of connections.
  - The middle layer: the db manager module uses the connection pool to perform specific CRUD operations (Create, Read, Update, Delete).
  - The top layer: the http server receives POST requests from the client, parses the request parameters, calls the db manager module to execute operations, and returns the results.
- Command Line Tool (CLI):
  - An HTTP client is used to parse command line arguments, construct the corresponding HTTP POST requests to send to the daemon, and output the response.

Following is the technology selection:

- Log: [zlog v1.2.18](https://github.com/HardySimpson/zlog.git)
- MySQL programming interface: [MySQL official C API v8.0.43](https://dev.mysql.com/doc/c-api/8.0/en/)
- HTTP server: [microhttpd v0.9.75](https://www.gnu.org/software/libmicrohttpd/)
- HTTP client: [libcurl 8_12_0](https://github.com/curl/curl)
- Unit tests: [unity v2.6.1](https://github.com/ThrowTheSwitch/Unity.git)

## Design

### MySQL connection pool

**Responsibilities**: Manage the creation, reuse, and destruction of MySQL connections

**Chat flow**:

```plain
┌──────────────────┐    ┌──────────────────────┐ No ┌────────────────────────────────┐
│  Connection req  │───▶│        In use?       │───▶│        Health checking?        │
└──────────────────┘    └──────────────────────┘    └────────────────────────────────┘
                                   │Yes                  │Yes        │No
                                   ▼                     │           ▼
                        ┌──────────────────────┐         │    ┌──────────────────────┐
                        │  Wait Condition var  │         │    │  Rebuild Connection  │
                        └──────────────────────┘         │    └──────────────────────┘
                                   │                     │           │
                                   ▼                     │           ▼
                        ┌──────────────────────┐         ▼    ┌──────────────────────┐
                    ◀───│  Return Connection   │◀─────────────│   Update Connection  │
                        └──────────────────────┘              └──────────────────────┘
```

**Key data sturcture**:

```c
typedef struct {
  MYSQL *mysql_conn;
  time_t last_used_time;
  bool in_use;
  int connection_id;
  char *host;
  char *user;
  char *password;
  char *database;
  unsigned int port;
} mysql_connection_t;

typedef struct {
  mysql_connection_t *connections; // shared resource, organizing by array
  int pool_size;
  int active_connections;
  pthread_mutex_t pool_mutex;
  pthread_cond_t connection_available;
  bool shutdown;
} connection_pool_t;
```

**Key interfaces**:

create / destroy connection pool, get / release connection, and health checking

```c
connection_pool_t *create_connection_pool(const char *host, const char *user, const char *password,
                                          const char *database, int pool_size);
void destroy_connection_pool(connection_pool_t *pool);
mysql_connection_t *get_connection(connection_pool_t *pool);
void release_connection(connection_pool_t *pool, mysql_connection_t *conn);
bool check_connection_health(mysql_connection_t *conn);
```

**core features**:

- Reuse of connections
  - The connection pool creates a certain number of database connections during initialization (specified by `pool_size`) and stores these connections in an array (by `connections`).
  - When a connection is needed, the `get_connection()` function searches the pool for an unused (`in_use` is false) and healthy connection.
  - The health of the connection is verified using the health check function `check_connection_health()` (implemented through `mysql_ping()`). If the connection is invalid, it attempts to reconnect (implemented through `mysql_close()` and `mysql_real_connect()` again).
  - After using the connection, the `release_connection()` function marks the connection as unused, updates the last used time, and notifies waiting threads through a condition variable (by `connection_available`).
- Thread-safe access
  - The connection_pool_t structure contains a mutex (by `pool_mutex`) and a condition variable (by `connection_available`).
  - When acquiring a connection (`get_connection()`) and releasing a connection (`release_connection()`), the mutex is used to protect shared data (such as the `connections` array, `active_connections`, etc.).
  - When there are no available connections, the thread that is trying to acquire a connection will wait on the condition variable (by `connection_available`) until a connection is released.
- Error handling
  - When creating a connection pool, if any connection fails to be created, a warning will be logged, but as long as at least one connection is successfully created, the connection pool will be created successfully.
  - When obtaining a connection, if the connection is not healthy, it will attempt to reconnect. If the reconnect fails, the connection will be skipped, and the search will continue for the next available connection.
  - If all connections are in use, the thread will block and wait until a connection is released.

### CRUD operations

**Responsibilities**:

Provide a unified interface for CRUD operations and manage the usage of database connections.

**Key data structures**:

```c
typedef struct {
  MYSQL_RES *mysql_res;
  int num_rows;
  int num_fields;
} db_result_t; // MySQL result set wrapper

typedef struct {
  connection_pool_t *conn_pool;
  char *last_error;
  int max_retries;
} db_manager_t;
```

**Key interfaces**:

create / destroy manager, and MySQL result set releasing, and CRUD operations.

```c
db_manager_t *db_manager_init(const char *host, const char *user, const char *password,
                              const char *database, int pool_size);
void db_manager_destroy(db_manager_t *manager);
void db_result_free(db_result_t *result);
int db_manager_create_row(db_manager_t *manager, const char *table, const char *data);
db_result_t *db_manager_read_row(db_manager_t *manager, const char *table, const char *where);
int db_manager_update_row(db_manager_t *manager, const char *table, const char *data,
                          const char *where);
int db_manager_delete_row(db_manager_t *manager, const char *table, const char *where);
```

**Core features**:

- Operation Executions:
  - Functions for CRUD operations are provided: `db_manager_create_row()`, `db_manager_read_row()`, `db_manager_update_row()`, and `db_manager_delete_row()`.
  - These functions construct the corresponding SQL statements and then execute them through the `db_manager_execute_common()` (a static function [src/db_manager.c:79](src/db_manager.c) in details) or `db_manager_execute_query()` (a static function [src/db_manager.c:129](src/db_manager.c) in details) functions.
  - During execution, connections are obtained from the connection pool, queries are executed, and then connections are released.
- Error Handling:
  - If an error occurs during execution, error information is logged and stored in the `last_error` field of the structure `db_manager_t`.
  - Retries will be attempted for retryable errors (such as a disconnected server connection), up to a maximum of `max_retries` field of the structure `db_manager_t`.

### HTTP server

**Responsibilities**:

Receive HTTP requests, parse parameters, perform database operations, and return responses.

**Key data structures**:

```c
typedef struct {
  struct MHD_Daemon *daemon;
  db_manager_t *db_mgr;
  bool running;
} http_server_t;
```

**Key interfaces**:

create / destroy server, and start / stop server.

```c
http_server_t *http_server_init(db_manager_t *db_mgr);
int http_server_start(http_server_t *server);
void http_server_stop(http_server_t *server);
void http_server_destroy(http_server_t *server);
```

**core features**:

- Request Processing:
  - An HTTP server is created using the libmicrohttpd library to listen on a specified port ([`HTTP_PORT`](src/macro.h)).
  - Only POST requests are processed, with the request body in `application/x-www-form-urlencoded` format, containing fields such as operation type (`operation`), table (`table`), data (`data`), and condition (`where`) in [src/key.h:3](src/key.h) in details.
  - The `post_data_iterator()` (a static function [src/http_server.c:76](src/http_server.c) in details) iterator is used to parse the POST data, and the parsed data is stored in the `connection_info_t` structure (a invisible data structure [src/http_server.c:13](src/http_server.c) in details).
  - After the request is processed, the `handle_db_request()` function (a static function [src/http_server.c:175](src/http_server.c) in details) is called to perform the corresponding database operation, and the result is returned to the client.
- Thread Safety:
  - Each connection has its own connection context (`connection_info_t`), which does not interfere with each other.
  - The database management module (`db_manager_t`) is thread-safe because it manages connections through a connection pool, and the connection pool is thread-safe.
- Error Handling:
  - If the POST data parsing fails, an error is returned.
  - If the database operation fails, a response containing error information is returned.

### HTTP client

**Responsibilities**:

Encapsulate HTTP requests, communicate with the daemon.

**Key data structures**:

```c
typedef struct {
  CURL *curl;
  char *base_url;
} http_client_t;
```

**Key interfaces**:

create / destroy client, and issues CRUD opeartions.

```c
http_client_t *http_client_init(const char *base_url);
void http_client_cleanup(http_client_t *client);
int http_client_create(http_client_t *client, const char *table, const char *data, char **output);
int http_client_read(http_client_t *client, const char *table, const char *where, char **output);
int http_client_update(http_client_t *client, const char *table, const char *data,
                       const char *where, char **output);
int http_client_delete(http_client_t *client, const char *table, const char *where, char **output);
```

**core features**:

- Request Sending:
  - Send an HTTP POST request using the libcurl library.
  - Encode the command line arguments as POST data and send it to the HTTP server of the daemon.
  - Parse the response and return the corresponding result based on the operation type (CREATE, READ, UPDATE, DELETE).
- Error Handling:
  - If the HTTP request fails, the error will be logged and -1 will be returned.
  - If the server returns an error response, the error information will be output.

## Unit tests

### Connection pool

The following is run condition, [test/test_connection_pool.c](test/test_connection_pool.c) in details.

```shell
$ cd build/test
$ ctest --verbose -R test_connection_pool
UpdateCTestConfiguration  from :/dbmanager/build/test/DartConfiguration.tcl
Test project /dbmanager/build/test
Constructing a list of tests
Done constructing a list of tests
Updating test list for fixtures
Added 0 tests to meet fixture requirements
Checking test dependency graph...
Checking test dependency graph end
test 1
    Start 1: test_connection_pool

1: Test command: /dbmanager/build/test/test_connection_pool
1: Working Directory: /dbmanager/build/test
1: Test timeout computed to be: 10000000
1: /dbmanager/test/test_connection_pool.c:107:test_create_connection_pool_success:PASS
1: /dbmanager/test/test_connection_pool.c:108:test_get_and_release_connection:PASS
1: /dbmanager/test/test_connection_pool.c:109:test_get_connection_from_shutdown_pool:PASS
1: /dbmanager/test/test_connection_pool.c:110:test_multiple_connections:PASS
1: /dbmanager/test/test_connection_pool.c:111:test_check_connection_health:PASS
1: /dbmanager/test/test_connection_pool.c:112:test_destroy_connection_pool:PASS
1: 
1: -----------------------
1: 6 Tests 0 Failures 0 Ignored 
1: OK
1/1 Test #1: test_connection_pool .............   Passed    0.04 sec

The following tests passed:
        test_connection_pool

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.06 sec
```

### CRUD functions

The unit tests need to use MySQL with user `root` and password `root` and database `mydb`. It will create and delete a table named `test_users` automatically in the process.

The following is run condition, [test/test_db_manager.c](test/test_db_manager.c) in details.

```shell
$ cd build/test
$ ctest --verbose -R test_db_manager                    
UpdateCTestConfiguration  from :/dbmanager/build/test/DartConfiguration.tcl
Test project /dbmanager/build/test
Constructing a list of tests
Done constructing a list of tests
Updating test list for fixtures
Added 0 tests to meet fixture requirements
Checking test dependency graph...
Checking test dependency graph end
test 2
    Start 2: test_db_manager

2: Test command: /dbmanager/build/test/test_db_manager
2: Working Directory: /dbmanager/build/test
2: Test timeout computed to be: 10000000
2: /dbmanager/test/test_db_manager.c:164:test_db_manager_init_success:PASS
2: /dbmanager/test/test_db_manager.c:165:test_db_manager_create_row_success:PASS
2: /dbmanager/test/test_db_manager.c:166:test_db_manager_create_row_invalid_params:PASS
2: /dbmanager/test/test_db_manager.c:167:test_db_manager_read_row_success:PASS
2: /dbmanager/test/test_db_manager.c:168:test_db_manager_read_row_invalid_params:PASS
2: /dbmanager/test/test_db_manager.c:169:test_db_manager_update_row_success:PASS
2: /dbmanager/test/test_db_manager.c:170:test_db_manager_update_row_invalid_params:PASS
2: /dbmanager/test/test_db_manager.c:171:test_db_manager_delete_row_success:PASS
2: /dbmanager/test/test_db_manager.c:172:test_db_manager_delete_row_invalid_params:PASS
2: /dbmanager/test/test_db_manager.c:173:test_db_manager_error_handling:PASS
2: 
2: -----------------------
2: 10 Tests 0 Failures 0 Ignored 
2: OK
1/1 Test #2: test_db_manager ..................   Passed    0.31 sec

The following tests passed:
        test_db_manager

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.32 sec
```

### Integration test

The unit tests need to use MySQL with user `root` and password `root` and database `mydb`. It will create and delete a table named `users` automatically in the process.

The following is run condition, [test/integration_tests.sh](test/integration_tests.sh) in details.

```shell
$ test/integration_tests.sh
mysql: [Warning] Using a password on the command line interface can be insecure.
数据库 mydb 已存在
mysql: [Warning] Using a password on the command line interface can be insecure.
表 users 已存在，正在删除...
mysql: [Warning] Using a password on the command line interface can be insecure.
正在创建表 users...
mysql: [Warning] Using a password on the command line interface can be insecure.
sleep 5s

---- CREATE ----
 Created 1 row(s)

---- READ ----
id             name           age            
---------------------------------------------
1              Alice          30             


---- UPDATE ----
 Updated 1 row(s)

---- DELETE ----
 Deleted 1 row(s)
```
