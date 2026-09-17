# Quy ước viết skill trong package này

Chạy `bash tools/lint.sh` trước mỗi lần commit. Lint chặn: liên kết chết, `name` không khớp
thư mục, tên skill cũ, file mồ côi, vượt ngân sách context.

## Khung SKILL.md bắt buộc

Mọi skill 01–12 theo đúng thứ tự mục này. Không thêm thế hệ template thứ hai.

```markdown
---
name: esp32-NN-<tên>          # PHẢI khớp tên thư mục
description: <250-450 ký tự>  # xem mục "Viết description" bên dưới
---

# NN — <Tên>

## Điều kiện vào
Chưa rõ chip / framework / phiên bản IDF → chạy Context Gate ở `esp32-firmware` trước.
Cổng an toàn và ngân sách context của `esp32-firmware` áp dụng nguyên vẹn tại đây.

<1-2 câu: skill này quyết định cái gì, đầu ra là gì>

## Nguyên tắc tối thượng: <một mệnh đề>
<vì sao; kèm bảng "những thứ bị cấm" nếu có hành vi sai điển hình>

## Quy trình N bước
<bảng: # | bước | đầu ra | bẫy thường gặp>

## <phần chuyên môn riêng của skill>
<hợp đồng API, bảng phân loại, checklist bắt buộc…>

## Mẫu báo cáo
<khối ``` để người dùng thấy được đầu ra sẽ trông thế nào>

## References
<mỗi dòng: `references/x.md` — một câu nói khi nào đọc>

## Không thuộc scope
<mỗi dòng: chủ đề → `esp32-NN-tên-skill` cụ thể, KHÔNG mô tả chung chung>

## Đầu ra
<cái gì được giao + mức kiểm chứng tối thiểu>
```

## Ngân sách (lint chặn)

| | Trần |
|---|---|
| SKILL.md | 200 dòng |
| Mỗi file trong `references/` và `checklists/` | 16 KB |
| Reference đọc mỗi lượt | 1 |

Vượt trần → **tách file**, không rút ngắn nội dung tới mức vô dụng.
Một reference nên trả lời được **một** câu hỏi trọn vẹn.

## Quy tắc nội dung

1. **Một chủ đề, một chủ sở hữu.** Muốn nhắc chủ đề của skill khác: trỏ tên file cụ thể,
   không chép nội dung. Trùng lặp là thứ sẽ lệch nhau sau vài lần sửa.
2. **Không sinh code trong orchestrator.** `esp32-firmware` chỉ điều phối.
3. **Mọi số đều phải có lý do.** "timeout 1000 ms" vô giá trị; "1000 ms — thời gian chuyển đổi
   theo datasheet là 750 ms" thì có.
4. **Nêu bẫy, không chỉ nêu cách đúng.** Phần giá trị nhất của mỗi file là cột "bẫy thường gặp".
5. **Cấm phải kèm lý do.** Một dòng "không làm X" mà không nói vì sao sẽ bị bỏ qua khi người
   dùng đang vội.
6. **Viết cho người đang gặp sự cố**, không viết cho người đang đọc tài liệu: đi thẳng vào
   triệu chứng → nguyên nhân → cách xử lý.

## Viết description

Description nằm trong system prompt của **mọi phiên**, kể cả phiên không liên quan ESP32 —
đây là chi phí cố định. Giữ 250–450 ký tự, cấu trúc:

```
<động từ + chủ đề> — <liệt kê từ khoá kích hoạt>. <Dùng khi: 2-3 tình huống cụ thể>.
```

Đủ từ khoá để định tuyến đúng, không liệt kê lại toàn bộ nội dung skill.
Chi tiết thuộc về thân file, nơi chỉ được nạp khi cần.

## Luật phân xử khi hai skill nói khác nhau

1. Cổng an toàn phần cứng thắng tất cả.
2. `esp32-10-security` thắng ở mọi vấn đề bảo mật.
3. Còn lại: skill sở hữu chủ đề thắng.

Phát hiện hai skill mâu thuẫn → **sửa ở một nơi và trỏ về**, đừng sửa cả hai cho khớp:
hai bản sao "đã khớp" sẽ lệch lại ở lần sửa sau.

## Thêm skill mới

1. Tạo `skills/esp32-13-<tên>/SKILL.md` theo khung trên.
2. Thêm **một dòng** vào Phase 1 (tín hiệu nhận biết) và **một dòng** vào Phase 2
   (routing + prerequisite) của `esp32-firmware/SKILL.md`.
3. Thêm mục "Không thuộc scope" ở các skill có chủ đề giáp ranh — chỉ dòng trỏ, không chép.
4. Thêm một dòng vào README.
5. `bash tools/lint.sh` phải xanh.
6. Ghi vào `CHANGELOG.md`, tăng `VERSION`.

## Sửa skill có sẵn

- Đổi một quy tắc → `grep -rn "<quy tắc>" skills/` xem nó được nhắc ở đâu, cập nhật **nơi sở hữu**
  và kiểm các nơi trỏ tới vẫn đúng.
- Thêm reference → liệt kê ở mục References của SKILL.md (lint bắt file mồ côi).
- Đổi tên file/skill → chạy lint, nó bắt được liên kết chết.
