#!/bin/bash

# 请在根目录下运行

set -e

MYSQL_USER="root"
MYSQL_PASSWORD="root"
MYSQL_HOST="localhost"
MYSQL_PORT="3306"

# 检查数据库 mydb 是否存在
DB_EXISTS=$(mysql -u$MYSQL_USER -p$MYSQL_PASSWORD -h$MYSQL_HOST -P$MYSQL_PORT -e "SHOW DATABASES LIKE 'mydb';" | grep "mydb" > /dev/null; echo $?)

if [ $DB_EXISTS -ne 0 ]; then
  echo "数据库 mydb 不存在，正在创建..."
  mysql -u$MYSQL_USER -p$MYSQL_PASSWORD -h$MYSQL_HOST -P$MYSQL_PORT -e "CREATE DATABASE mydb;"
else
  echo "数据库 mydb 已存在"
fi

# 检查数据库 mydb 中是否存在表 users
TABLE_EXISTS=$(mysql -u$MYSQL_USER -p$MYSQL_PASSWORD -h$MYSQL_HOST -P$MYSQL_PORT -Dmydb -e "SHOW TABLES LIKE 'users';" | grep "users" > /dev/null; echo $?)

if [ $TABLE_EXISTS -ne 0 ]; then
  echo "表 users 不存在，正在创建..."
  mysql -u$MYSQL_USER -p$MYSQL_PASSWORD -h$MYSQL_HOST -P$MYSQL_PORT -Dmydb -e "CREATE TABLE users (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100), age INT);"
else
  echo "表 users 已存在，正在删除..."
  mysql -u$MYSQL_USER -p$MYSQL_PASSWORD -h$MYSQL_HOST -P$MYSQL_PORT -Dmydb -e "DROP TABLE users;"
  echo "正在创建表 users..."
  mysql -u$MYSQL_USER -p$MYSQL_PASSWORD -h$MYSQL_HOST -P$MYSQL_PORT -Dmydb -e "CREATE TABLE users (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100), age INT);"
fi

# 执行测试
build/dbmanager --db-host=$MYSQL_HOST \
            --db-user=$MYSQL_USER \
            --db-password=$MYSQL_PASSWORD \
            --db-name=mydb \
            --pool-size=8 > /dev/null 2>&1 &
PID=$!
echo "sleep 5s"
sleep 5
echo -e "\n---- CREATE ----"
build/dbcli create --table=users --data="name='Alice',age=30"
echo -e "\n---- READ ----"
build/dbcli read   --table=users --where="id=1"
echo -e "\n---- UPDATE ----"
build/dbcli update --table=users --data="age=31" --where="id=1"
echo -e "\n---- DELETE ----"
build/dbcli delete --table=users --where="id=1"
kill $PID
