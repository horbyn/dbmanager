FROM ubuntu:jammy

# 禁止交互式时区设置参考：https://serverfault.com/a/1016972
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# x86_64 或 aarch64
ARG ARCH

RUN apt-get update && apt install -y --no-install-recommends ca-certificates && \
    cp /etc/apt/sources.list /etc/apt/sources.list.bak && \
    if [ "$ARCH" = "aarch64" ]; then \
        sed -i -e 's@//ports.ubuntu.com/\? @//ports.ubuntu.com/ubuntu-ports @g' \
            -e 's@//ports.ubuntu.com@//mirrors.ustc.edu.cn@g' \
            /etc/apt/sources.list; \
    elif [ "$ARCH" = "x86_64" ]; then \
        sed -i 's@//.*archive.ubuntu.com@//mirrors.ustc.edu.cn@g' /etc/apt/sources.list; \
        sed -i 's/security.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list; \
        sed -i 's/http:/https:/g' /etc/apt/sources.list; \
    else \
        echo "未知架构，无法设置 apt 代理"; \
        exit 1; \
    fi && \
    apt-get update && apt install -y --no-install-recommends \
        sudo vim wget curl git build-essential gdb systemd tzdata init \
        mysql-server libmysqlclient-dev libmicrohttpd-dev \
        libffi-dev libunistring-dev libpsl-dev libnghttp2-dev && \
    rm -rf /var/lib/apt/lists/* && \
    apt-get clean && \
    # mac 全局忽略 .DS_Store 配置文件
    # ref to: https://orianna-zzo.github.io/sci-tech/2018-01/mac%E4%B8%ADgit%E5%BF%BD%E7%95%A5.ds_store%E6%96%87%E4%BB%B6/
    echo "# Mac OS specified" > ~/.gitignore_global && \
    echo "**/.DS_Store" > ~/.gitignore_global && \
    git config --global core.excludesfile ~/.gitignore_global

CMD ["/bin/bash"]
