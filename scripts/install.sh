#!/bin/bash

set -e

# 目前仅支持 Ubuntu
if ! [[ $(cat /etc/os-release | grep "^NAME=" | cut -d'=' -f2 | tr -d '"') == "Ubuntu" ]]; then
  echo "当前系统不是 Ubuntu"
  exit 1
elif [[ $(cat /etc/os-release | grep "^VERSION=" | cut -d'=' -f2 | tr -d '"' | awk '{print $1}') < "22.04" ]]; then
  echo "当前系统版本低于 Ubuntu 22.04"
  exit 1
fi

SERVICE_NAME="dbmanager.service"
if systemctl list-units --type=service --all | grep -q "$SERVICE_NAME"; then
  echo "$SERVICE_NAME is installed. Stopping the service..."
  systemctl stop $SERVICE_NAME
else
  echo "$SERVICE_NAME is not installed. Skipping stop..."
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(dirname "$SCRIPT_DIR")
INSTALL_DIR="/opt/dbmngr"
INSTALL_LIB_DIR="/opt/dbmngr/lib"
INSTALL_BIN_DIR="/opt/dbmngr/bin"
mkdir -p $INSTALL_DIR
mkdir -p $INSTALL_LIB_DIR
mkdir -p $INSTALL_BIN_DIR

TARGET_BIN="$PROJECT_DIR/dbcli"
TARGET_DAEMON="$PROJECT_DIR/dbmanager"

cp -v "$TARGET_BIN" "$INSTALL_BIN_DIR/"
cp -v "$TARGET_BIN" "/usr/local/bin/"
cp -v "$TARGET_DAEMON" "$INSTALL_BIN_DIR/"
cp -v $PROJECT_DIR/libs/* "$INSTALL_LIB_DIR/"
ldconfig
cp -v "$PROJECT_DIR/conf/dbmanager.service" "/etc/systemd/system/"
cp -v "$PROJECT_DIR/conf/dbmanager.conf" "/etc/"
cp -v "$PROJECT_DIR/scripts/install.sh" "$INSTALL_DIR/"
cp -v "$PROJECT_DIR/scripts/uninstall.sh" "$INSTALL_DIR/"
chmod +x "$INSTALL_BIN_DIR/dbcli"

systemctl daemon-reload
systemctl enable $SERVICE_NAME
systemctl start $SERVICE_NAME

echo "✅ 安装 $INSTALL_BIN_DIR/dbcli、$INSTALL_BIN_DIR/dbmanager、\
/etc/dbmanager.conf、/usr/local/bin/dbcli 和 /etc/systemd/system/dbmanager.service 完成"
