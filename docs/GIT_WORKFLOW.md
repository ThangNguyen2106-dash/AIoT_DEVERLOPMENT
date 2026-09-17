# Quy trình Git cho dự án AIoT_DEVERLOPMENT

Tài liệu mô tả mô hình fork + Pull Request mà dự án sử dụng.

## 1. Mô hình 3 kho

```
ThangNguyen2106-dash/AIoT_DEVERLOPMENT   <- upstream (kho gốc)
            | fork
            v
<tai-khoan-cua-ban>/AIoT_DEVERLOPMENT    <- origin (fork cá nhân)
            | clone
            v
máy local
```

Quy ước tên remote:

| Remote     | Trỏ tới    | Dùng để          |
|------------|------------|------------------|
| `origin`   | fork của bạn | push             |
| `upstream` | kho gốc      | fetch / pull     |

Thiết lập lần đầu:

```bash
git clone https://github.com/<tai-khoan-cua-ban>/AIoT_DEVERLOPMENT.git
cd AIoT_DEVERLOPMENT
git remote add upstream https://github.com/ThangNguyen2106-dash/AIoT_DEVERLOPMENT.git
git remote -v
```

## 2. Vòng lặp làm việc

```bash
# B1 - đồng bộ với kho gốc trước khi bắt đầu
git checkout main
git fetch upstream
git merge upstream/main
git push origin main

# B2 - mỗi công việc một nhánh riêng, không làm trên main
git checkout -b feat/ten-tinh-nang

# B3 - commit từng thay đổi nhỏ có nghĩa
git add <file>
git commit -m "feat(scope): mô tả ngắn"

# B4 - đẩy nhánh lên fork
git push -u origin feat/ten-tinh-nang

# B5 - mở Pull Request sang kho gốc
gh pr create --repo ThangNguyen2106-dash/AIoT_DEVERLOPMENT \
  --base main --head <tai-khoan-cua-ban>:feat/ten-tinh-nang \
  --title "feat(scope): mô tả ngắn" \
  --body "Nội dung thay đổi. Closes #<số issue>"
```

## 3. Cập nhật PR theo góp ý review

Không tạo PR mới. Commit tiếp vào cùng nhánh rồi push, PR tự cập nhật:

```bash
git checkout feat/ten-tinh-nang
git commit -am "fix(scope): chỉnh sửa theo review"
git push
```

## 4. Nhánh bị cũ so với upstream

```bash
git fetch upstream
git rebase upstream/main
# nếu conflict: sửa file -> git add <file> -> git rebase --continue
git push --force-with-lease
```

Luôn dùng `--force-with-lease` thay cho `--force`: lệnh sẽ từ chối nếu có người khác vừa đẩy commit lên nhánh đó.

## 5. Dọn dẹp sau khi PR được merge

```bash
git checkout main
git fetch upstream && git merge upstream/main
git branch -d feat/ten-tinh-nang
git push origin --delete feat/ten-tinh-nang
```

## 6. Quy ước commit

Dự án dùng Conventional Commits:

```
<type>(<scope>): <mô tả ngắn, không viết hoa đầu câu, không dấu chấm cuối>
```

Các `type` thường dùng: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`.

Ví dụ:

```
feat(driver): add BME280 I2C driver
fix(wifi): prevent MQTT lockup on reconnect
docs(git): add fork workflow guide
```

## 7. Nguyên tắc bắt buộc

1. Không commit trực tiếp lên `main`.
2. Một nhánh chỉ phục vụ một mục đích; không gộp nhiều thay đổi không liên quan vào một PR.
3. Luôn `git fetch upstream` trước khi tạo nhánh mới.
4. Không `--force` lên nhánh chung (`main`, `PLG`, `THANG`).
5. Không commit secret (API key, mật khẩu Wi-Fi, chứng chỉ). Dùng `SECRETS.example` làm mẫu.
