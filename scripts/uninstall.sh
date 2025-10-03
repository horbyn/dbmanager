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
  systemctl disable $SERVICE_NAME
else
  echo "$SERVICE_NAME is not installed. Skipping stop..."
fi

output_redirect="> /dev/null"
INSTALL_DIR="/opt/dbmngr"
INSTALL_BIN_DIR="/opt/dbmngr/bin"

if [[ -d "$INSTALL_DIR" ]]; then
  rm -r -v "$INSTALL_DIR"
fi

if [[ -f "/etc/systemd/system/dbmanager.service" ]]; then
  rm -v "/etc/systemd/system/dbmanager.service"
fi

if [[ -f /usr/local/bin/dbcli ]]; then
  rm -v /usr/local/bin/dbcli
fi

if [[ -f /etc/dbmanager.conf ]]; then
  rm -v /etc/dbmanager.conf
fi

# 输出卸载完成信息
echo "✅ 卸载完成: $INSTALL_BIN_DIR/dbcli、$INSTALL_BIN_DIR/dbmanager、\
/etc/dbmanager.conf、/usr/local/bin/dbcli 和 /etc/systemd/system/dbmanager.service 已被删除"
