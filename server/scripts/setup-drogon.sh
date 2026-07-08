#!/usr/bin/env bash
# 拉取并编译 Drogon（及 chatlive 所需依赖）到 server/third_party/local
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TP="$ROOT/third_party"
LOCAL="$TP/local"
DROGON="$TP/drogon"
DROGON_TAG="${DROGON_TAG:-v1.9.13}"

echo "==> third_party root: $TP"

mkdir -p "$TP"
if [[ ! -d "$DROGON/.git" ]]; then
  echo "==> Cloning Drogon ($DROGON_TAG) ..."
  git clone --recursive --branch "$DROGON_TAG" https://github.com/drogonframework/drogon.git "$DROGON"
else
  echo "==> Updating Drogon ..."
  git -C "$DROGON" fetch --tags
  git -C "$DROGON" checkout "$DROGON_TAG" 2>/dev/null || git -C "$DROGON" pull
  git -C "$DROGON" submodule update --init --recursive
fi

# 可选：系统包（需 sudo）
if command -v apt-get >/dev/null && [[ "${SKIP_APT:-}" != "1" ]]; then
  if ! dpkg -s libjsoncpp-dev >/dev/null 2>&1; then
    echo "==> 建议安装系统依赖（需 sudo）："
    echo "    sudo apt-get install -y libjsoncpp-dev libssl-dev zlib1g-dev libbrotli-dev libpq-dev uuid-dev"
    echo "    或设置 SKIP_APT=1 使用脚本内本地编译的 libuuid/libpq"
  fi
fi

# jsoncpp / zlib / brotli（无 sudo 时用 apt download 解压）
if [[ ! -f "$LOCAL/usr/include/jsoncpp/json/json.h" ]]; then
  echo "==> Fetching jsoncpp/zlib/brotli debs into $LOCAL ..."
  mkdir -p /tmp/chatlive-apt && cd /tmp/chatlive-apt
  apt-get download libjsoncpp-dev libjsoncpp25 libbrotli-dev zlib1g-dev 2>/dev/null || true
  for f in *.deb; do [[ -f "$f" ]] && dpkg -x "$f" "$LOCAL"; done
fi

# libuuid（util-linux）
if [[ ! -f "$LOCAL/include/uuid/uuid.h" ]]; then
  echo "==> Building libuuid ..."
  UL="$TP/util-linux-2.40.2"
  if [[ ! -d "$UL" ]]; then
    cd "$TP"
    wget -q https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.40/util-linux-2.40.2.tar.xz
    tar xf util-linux-2.40.2.tar.xz
  fi
  cd "$UL"
  [[ -f config.status ]] || ./configure --prefix="$LOCAL" --disable-all-programs --enable-libuuid
  make -j"$(nproc)" libuuid.la
  mkdir -p "$LOCAL/include/uuid" "$LOCAL/lib"
  cp -a libuuid/src/uuid.h "$LOCAL/include/uuid/"
  cp -a .libs/libuuid.so* "$LOCAL/lib/"
fi

# libpq
if [[ ! -f "$LOCAL/include/libpq-fe.h" && ! -f "$LOCAL/include/postgresql/libpq-fe.h" ]]; then
  echo "==> Building libpq ..."
  PG="$TP/postgresql-16.4"
  if [[ ! -d "$PG" ]]; then
    cd "$TP"
    wget -q https://ftp.postgresql.org/pub/source/v16.4/postgresql-16.4.tar.bz2
    tar xf postgresql-16.4.tar.bz2
  fi
  cd "$PG"
  export LDFLAGS="-L$LOCAL/lib -Wl,-rpath,$LOCAL/lib"
  export CPPFLAGS="-I$LOCAL/include"
  [[ -f config.status ]] || ./configure --prefix="$LOCAL" --without-readline --without-icu --with-openssl --with-uuid=e2fs
  make -C src/interfaces/libpq -j"$(nproc)"
  make -C src/interfaces/libpq install
  make -C src/include install
fi

echo "==> Configuring & building Drogon ..."
rm -rf "$DROGON/build"
mkdir -p "$DROGON/build"
cd "$DROGON/build"
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$LOCAL" \
  -DCMAKE_PREFIX_PATH="$LOCAL;$LOCAL/usr" \
  -DJSONCPP_INCLUDE_DIRS="$LOCAL/usr/include/jsoncpp" \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_CTL=OFF \
  -DBUILD_ORM=ON \
  -DBUILD_POSTGRESQL=ON \
  -DBUILD_BROTLI=ON \
  -DBUILD_YAML_CONFIG=OFF

make -j"$(nproc)"
make install

echo ""
echo "Done. Drogon installed to: $LOCAL"
echo "Build chatlive-server:"
echo "  export CMAKE_PREFIX_PATH=$LOCAL"
echo "  export LD_LIBRARY_PATH=$LOCAL/lib:$LOCAL/usr/lib/x86_64-linux-gnu:\$LD_LIBRARY_PATH"
echo "  cd $ROOT/chatlive/build && cmake .. && make -j\$(nproc)"
