set -euo pipefail

PKGVER="255.4-1ubuntu8.10"
ARCH="amd64"

# 需要统一版本的包
CORE_PKGS=(
  "libsystemd0"
  "libsystemd-shared"
  "libudev1"
  "systemd"
  "systemd-sysv"
  "systemd-resolved"
  "libnss-systemd"
  "libpam-systemd"
  "udev"
)

CACHE="/var/cache/apt/archives"
WORK="/tmp/systemd-fix"
mkdir -p "$WORK"
cd "$WORK"

# 如可联网，尝试抓取缺失的 .deb；不行就跳过，用已有缓存或你手动放的文件
download_pkg () {
  local p="$1"
  local f="${p}_${PKGVER}_${ARCH}.deb"
  if [ ! -f "$f" ] && [ -f "$CACHE/$f" ]; then
    cp -f "$CACHE/$f" .
  fi
  if [ ! -f "$f" ]; then
    if command -v apt-get >/dev/null 2>&1; then
      set +e
      apt-get download "${p}=${PKGVER}" >/dev/null 2>&1
      set -e
      # apt-get download 会直接生成当前目录的包
    fi
  fi
}

echo "[*] 收集需要的 .deb 包（如无法联网，此步会尽力使用缓存/你手动提供的文件）"
for p in "${CORE_PKGS[@]}"; do
  download_pkg "$p" || true
done

# 检查必需包是否齐全
MISSING=0
for p in "${CORE_PKGS[@]}"; do
  f="${p}_${PKGVER}_${ARCH}.deb"
  if [ ! -f "$f" ]; then
    echo "[!] 缺少 $f"
    MISSING=1
  fi
done
if [ "$MISSING" -ne 0 ]; then
  echo
  echo "请把缺失的 ${PKGVER} 版本 .deb 放到当前目录或 $CACHE 后重试。"
  exit 1
fi

echo "[*] 处理 systemd / udev：移除 preinst 以绕过误判"
fix_pkg () {
  local f="$1"
  local dir="$WORK/${f%.deb}"
  rm -rf "$dir"
  mkdir -p "$dir/EX"/{c,control}
  # 解包内容与控制信息
  dpkg-deb -x "$f" "$dir/EX/c"
  dpkg-deb -e "$f" "$dir/EX/control"
  # 删 preinst（如果有）
  if [ -f "$dir/EX/control/preinst" ]; then
    rm -f "$dir/EX/control/preinst"
  fi
  (cd "$dir/EX" && dpkg-deb -b . "$WORK/fixed-$f" >/dev/null)
  echo "$WORK/fixed-$f"
}

FIXED_SYSTEMD="$(fix_pkg "systemd_${PKGVER}_${ARCH}.deb")"
FIXED_UDEV="$(fix_pkg "udev_${PKGVER}_${ARCH}.deb")"

echo "[*] 开始安装（先库、后服务包）"
# 先装库：libsystemd0, libsystemd-shared, libudev1
sudo dpkg -i "./libsystemd0_${PKGVER}_${ARCH}.deb" "./libsystemd-shared_${PKGVER}_${ARCH}.deb" "./libudev1_${PKGVER}_${ARCH}.deb" || true

# 再装 systemd 栈：systemd, systemd-sysv, systemd-resolved, libnss-systemd, libpam-systemd, udev
sudo dpkg -i "$FIXED_SYSTEMD" \
  "./systemd-sysv_${PKGVER}_${ARCH}.deb" \
  "./systemd-resolved_${PKGVER}_${ARCH}.deb" \
  "./libnss-systemd_${PKGVER}_${ARCH}.deb" \
  "./libpam-systemd_${PKGVER}_${ARCH}.deb" \
  "$FIXED_UDEV" || true

echo "[*] 依赖收尾"
sudo apt-get -f install -y

echo "[*] 建议暂时 hold，以免内部源再次推送不一致版本"
sudo apt-mark hold systemd systemd-sysv systemd-resolved libsystemd0 libsystemd-shared udev libudev1 libnss-systemd libpam-systemd || true

echo "[✓] 完成。用以下命令确认版本："
echo "    dpkg -l | egrep 'systemd|udev' | awk '{print \$1, \$2, \$3}'"
