#!/usr/bin/env bash
# Cài bộ ESP32 Firmware Skills từ package này vào Claude Code (macOS / Linux / Git Bash).
#
#   ./install.sh                         # cài toàn cục vào ~/.claude/skills
#   ./install.sh /path/to/project        # cài riêng cho một dự án
#   ./install.sh --uninstall             # gỡ khỏi ~/.claude/skills
#
# Idempotent: chạy lại để đồng bộ. Chỉ đụng các thư mục esp32-*, không ảnh hưởng skill khác.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$here/skills"
[ -d "$src" ] || { echo "Không tìm thấy thư mục nguồn: $src" >&2; exit 1; }

uninstall=0
dst="$HOME/.claude/skills"

case "${1:-}" in
  --uninstall) uninstall=1 ;;
  "")          ;;
  *)           dst="$(cd "$1" && pwd)/.claude/skills" ;;
esac

mkdir -p "$dst"
find "$dst" -maxdepth 1 -name 'esp32-*' -exec rm -rf {} +

if [ "$uninstall" -eq 1 ]; then
  echo "Đã gỡ bộ ESP32 skills khỏi: $dst"
  exit 0
fi

n=0
for d in "$src"/*/; do
  cp -r "$d" "$dst/"
  n=$((n + 1))
done

echo "Đã cài $n skill vào: $dst"
echo "Khởi động lại Claude Code để nhận diện skill mới."
