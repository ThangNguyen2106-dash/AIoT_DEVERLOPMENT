#!/usr/bin/env bash
# Lint cho package ESP32 skills. Chạy từ đâu cũng được.
#
#   ./tools/lint.sh
#
# Kiểm 6 thứ:
#   1. frontmatter: có `name`, và `name` khớp tên thư mục
#   2. liên kết nội bộ: mọi `references/x.md`, `checklists/x.md`, `templates/x.md` đều tồn tại
#   3. liên kết chéo skill: `esp32-NN-xxx/...` trỏ tới skill và file có thật
#   4. tên skill: không còn tên phiên bản cũ (không có tiền tố NN-)
#   5. file mồ côi: file trong references/ mà SKILL.md không liệt kê
#   6. ngân sách: SKILL.md <= 200 dòng, reference <= 16 KB

set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here/skills" || { echo "Không tìm thấy $here/skills" >&2; exit 1; }

fail=0
err() { echo "  FAIL  $*"; fail=1; }

valid_skills=$(ls -d */ | tr -d '/')

echo "== 1. frontmatter =="
for sk in $valid_skills; do
  f="$sk/SKILL.md"
  [ -f "$f" ] || { err "$sk: thiếu SKILL.md"; continue; }
  name=$(sed -n 's/^name: *//p' "$f" | head -1)
  [ -n "$name" ] || err "$f: thiếu trường name"
  [ "$name" = "$sk" ] || err "$f: name='$name' không khớp thư mục '$sk'"
  desc=$(sed -n 's/^description: *//p' "$f" | head -1)
  [ -n "$desc" ] || err "$f: thiếu trường description"
done

echo "== 2. liên kết nội bộ =="
for sk in $valid_skills; do
  grep -rhoE '`(references|checklists|templates)/[A-Za-z0-9._-]+`' "$sk" 2>/dev/null \
    | tr -d '`' | sort -u | while read -r p; do
      [ -e "$sk/$p" ] || echo "  FAIL  $sk -> $p (không tồn tại)"
    done
done | tee /tmp/esp32lint2 ; grep -q FAIL /tmp/esp32lint2 && fail=1

echo "== 3. liên kết chéo skill =="
grep -rhoE 'esp32-[0-9]{2}-[a-z-]+/(references|checklists|templates)/[A-Za-z0-9._-]+\.md' . \
  | sort -u | while read -r p; do
      [ -e "$p" ] || echo "  FAIL  $p (không tồn tại)"
    done | tee /tmp/esp32lint3 ; grep -q FAIL /tmp/esp32lint3 && fail=1

echo "== 4. tên skill cũ =="
grep -rnoE 'esp32-(project-setup|security|debug|testing|drivers|connectivity|hardware|architecture|app|performance|power|release)/' . \
  | tee /tmp/esp32lint4 | sed 's/^/  FAIL  tên skill cũ: /'
[ -s /tmp/esp32lint4 ] && fail=1

echo "== 5. file mồ côi =="
for sk in $valid_skills; do
  find "$sk" -mindepth 2 -name '*.md' 2>/dev/null | while read -r f; do
    b="${f#$sk/}"
    grep -q "$b" "$sk/SKILL.md" || echo "  FAIL  $f không được liệt kê trong $sk/SKILL.md"
  done
done | tee /tmp/esp32lint5 ; grep -q FAIL /tmp/esp32lint5 && fail=1

echo "== 6. ngân sách context =="
for sk in $valid_skills; do
  n=$(wc -l < "$sk/SKILL.md")
  [ "$n" -le 200 ] || err "$sk/SKILL.md: $n dòng (trần 200)"
done
find . -path '*/references/*.md' -o -path '*/checklists/*.md' | while read -r f; do
  b=$(wc -c < "$f")
  [ "$b" -le 16384 ] || echo "  FAIL  $f: $b byte (trần 16384)"
done | tee /tmp/esp32lint6 ; grep -q FAIL /tmp/esp32lint6 && fail=1

rm -f /tmp/esp32lint[23456]
echo
if [ "$fail" -eq 0 ]; then echo "LINT OK — $(echo "$valid_skills" | wc -w) skill"; else echo "LINT CÓ LỖI"; fi
exit "$fail"
