#!/bin/bash

set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_DIR=$(dirname "$SCRIPT_DIR")
PKG_DIR=release

if [ ! -d "$PROJECT_DIR/scripts" ] || [ ! -d "$PROJECT_DIR/$PKG_DIR" ]; then
  echo "错误：缺少打包文件" >&2
  exit 1
fi

LIBP11KIT=/lib/$(uname -m)-linux-gnu/libp11-kit.so.0
LIBSTDCXX=/lib/$(uname -m)-linux-gnu/libstdc++.so.6
LIBFFI=/lib/$(uname -m)-linux-gnu/libffi.so.8
if [ ! -f "$LIBP11KIT" ] || [ ! -f "$LIBSTDCXX" ] || [ ! -f "$LIBFFI" ]; then
  echo "错误：缺少 libp11-kit.so.0 或 libstdc++.so.6 或 libffi.so.8" >&2
  exit 1
fi

trap 'echo "😯 清理临时目录 $tmp_dir"; rm -rf "$tmp_dir"\n' EXIT

tmp_dir=$(mktemp -d -t dbmngr_pack_XXXXXX)
mkdir -p "$tmp_dir/dbmngr"
mkdir -p "$tmp_dir/dbmngr/conf"
mkdir -p "$tmp_dir/dbmngr/libs"
mkdir -p "$tmp_dir/dbmngr/scripts"
mkdir -p "$PROJECT_DIR/packages"

# 复制必需文件到临时目录
cp -v -f "$PROJECT_DIR/$PKG_DIR/dbmanager" "$tmp_dir/dbmngr/"
cp -v -f "$PROJECT_DIR/$PKG_DIR/dbcli" "$tmp_dir/dbmngr/"
cp -v -f $PROJECT_DIR/conf/* "$tmp_dir/dbmngr/conf/"
cp -v -f "$LIBP11KIT" "$tmp_dir/dbmngr/libs/"
cp -v -f "$LIBSTDCXX" "$tmp_dir/dbmngr/libs/"
cp -v -f "$LIBFFI" "$tmp_dir/dbmngr/libs/"
cp -v -f "$PROJECT_DIR/scripts/install.sh" "$tmp_dir/dbmngr/scripts/"
cp -v -f "$PROJECT_DIR/scripts/uninstall.sh" "$tmp_dir/dbmngr/scripts/"

chmod +x $tmp_dir/dbmngr/scripts/*.sh
PKG=$PROJECT_DIR/packages/dbmngr.$(uname -m).tar.gz
tar -czf "$PKG" -C "$tmp_dir" .

echo "✅ 打包完成：$PKG"
echo "包含文件："
tar -ztvf "$PKG" | awk '{print $6}'