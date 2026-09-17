# TCP / UDP thuần (BSD socket trên lwIP)

Chỉ dùng socket thô khi thật sự cần: giao thức của bên thứ ba (Modbus TCP, giao thức nhà máy),
độ trễ cực thấp trong LAN, hoặc multicast/broadcast. Với cloud thì MQTT/HTTPS hầu như luôn tốt hơn.

## TCP client — connect có timeout

`connect()` mặc định là blocking và có thể treo hàng chục giây. Cách duy nhất đặt được timeout
cho connect là non-blocking + `select()`:

```c
static int tcp_connect_to(const char *host, uint16_t port, int timeout_ms, net_fail_t *cls)
{
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM }, *res;
    char portstr[6]; snprintf(portstr, sizeof portstr, "%u", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0) { *cls = NET_FAIL_LINK; return -1; }

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) { *cls = NET_FAIL_INIT; freeaddrinfo(res); return -1; }   /* hết socket/heap */

    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl | O_NONBLOCK);

    int rc = connect(s, res->ai_addr, res->ai_addrlen);
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set w; FD_ZERO(&w); FD_SET(s, &w);
        struct timeval tv = { .tv_sec = timeout_ms / 1000,
                              .tv_usec = (timeout_ms % 1000) * 1000 };
        rc = select(s + 1, NULL, &w, NULL, &tv);
        if (rc == 0) { *cls = NET_FAIL_LINK; goto fail; }          /* timeout */
        int soerr = 0; socklen_t l = sizeof soerr;
        getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &l);           /* select() báo sẵn sàng KHÔNG nghĩa là thành công */
        if (soerr != 0) { errno = soerr; *cls = classify_errno(soerr); goto fail; }
    } else if (rc < 0) { *cls = classify_errno(errno); goto fail; }

    fcntl(s, F_SETFL, fl);                                          /* về blocking + có SO_RCVTIMEO */
    freeaddrinfo(res);
    *cls = NET_OK;
    return s;
fail:
    close(s); freeaddrinfo(res); return -1;
}
```

Bỏ bước `getsockopt(SO_ERROR)` là lỗi tinh vi hay gặp: `select()` báo writable cả khi connect
thất bại, và code sẽ tưởng đã kết nối rồi gửi vào socket chết.

## Socket option bắt buộc

```c
struct timeval t = { .tv_sec = 5 };
setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);   /* recv() không bao giờ chờ vĩnh viễn */
setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof t);

int one = 1;
setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);  /* tắt Nagle cho gói nhỏ, độ trễ thấp */

/* keepalive: phát hiện half-open (phía kia biến mất không đóng) */
setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof one);
int idle = 30, intvl = 5, cnt = 3;
setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof idle);
setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof intvl);
setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof cnt);
```

Không có `SO_RCVTIMEO`, một `recv()` sẽ khoá task vĩnh viễn khi phía kia im lặng — task
watchdog bắn, và nguyên nhân rất khó truy.

## Đọc/ghi cho đúng

```c
/* send() có thể gửi MỘT PHẦN — phải lặp */
static int send_all(int s, const uint8_t *p, size_t n)
{
    size_t off = 0;
    while (off < n) {
        int k = send(s, p + off, n - off, 0);
        if (k > 0) { off += k; continue; }
        if (k < 0 && (errno == EINTR)) continue;
        return -1;                       /* EAGAIN ở đây = SO_SNDTIMEO hết hạn → LINK_FAIL */
    }
    return 0;
}
```

- `recv()` trả **0** nghĩa là phía kia đã đóng sạch (EOF) — **không phải** lỗi, nhưng phải đóng
  socket và reconnect. Trả `-1` với `EAGAIN` là hết timeout đọc; xử lý khác hẳn EOF.
- TCP là **dòng byte, không phải gói**. Một `recv()` có thể trả nửa message hoặc hai message
  dính nhau. Mọi giao thức trên TCP phải có khung: độ dài ở đầu, hoặc ký tự kết thúc, hoặc kích
  thước cố định. Giả định "một recv = một message" là bug chắc chắn xảy ra khi mạng bận.
- Parser theo khung phải có trần độ dài; nhận `len = 0xFFFFFFFF` từ dây rồi `malloc` theo nó là
  lỗ hổng (→ `esp32-10-security`).

## UDP

```c
int s = socket(AF_INET, SOCK_DGRAM, 0);
struct timeval t = { .tv_sec = 2 };
setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);
```

- UDP **không đảm bảo** đến nơi, đến đúng thứ tự, hay đến một lần. Nếu ứng dụng cần chắc chắn
  thì phải tự làm: số thứ tự, ACK, retry, khử trùng lặp — đến lúc đó nên cân nhắc dùng TCP.
- Giới hạn payload thực tế: giữ ≤ ~1400 byte để không bị phân mảnh IP. Gói phân mảnh rớt một
  mảnh là mất cả gói.
- `recvfrom()` luôn trả trọn một datagram (hoặc cắt cụt nếu buffer nhỏ — kiểm tra và bỏ). Khác
  hẳn TCP.
- Broadcast cần `SO_BROADCAST`; multicast cần join group qua `IP_ADD_MEMBERSHIP` và phải join
  lại sau mỗi lần có IP mới (rớt Wi-Fi rồi vào lại là mất membership — nguyên nhân số một của
  "mDNS/discovery chạy được một lần rồi thôi").
- Dữ liệu UDP đến từ bất kỳ ai trong mạng: luôn kiểm tra địa chỉ nguồn và nội dung trước khi dùng.

## Số socket có hạn

`CONFIG_LWIP_MAX_SOCKETS` mặc định 10 (tính cả socket do MQTT/HTTP/TLS dùng). Rò socket biểu
hiện là `socket()` trả `ENOMEM`/`ENFILE` sau vài giờ chạy. Mỗi đường thoát khỏi hàm đều phải
`close()`; `goto fail` một lối ra duy nhất là cách viết an toàn nhất ở đây.

## Ánh xạ errno → lớp lỗi
Xem bảng đầy đủ ở `connection-contract.md` §1.
