#ifndef WEBUI_HPP
#define WEBUI_HPP

#include <Arduino.h>

class WebUI
{
public:
    static String ConfigPage(
        const String &mac,
        const String &curWifi,
        const String &user,
        const String &pass)
    {
        String html;
        html.reserve(3200);

        html = F(
            "<!DOCTYPE html><html lang='vi'><head>"
            "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
            "<title>AIoT - Cấu Hình Thiết Bị</title>"
            "<style>"
            ":root{--primary:#2563eb;--primary-hover:#1d4ed8;--bg:#f8fafc;--card:#ffffff;--text:#1e293b;--subtext:#64748b;--border:#e2e8f0;--success:#10b981;--radius:12px}"
            "*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,sans-serif}"
            "body{background:var(--bg);color:var(--text);display:flex;justify-content:center;align-items:center;min-height:100vh;padding:16px}"
            ".card{background:var(--card);width:100%;max-width:420px;border-radius:var(--radius);padding:24px;box-shadow:0 10px 25px -5px rgba(0,0,0,0.05),0 8px 10px -6px rgba(0,0,0,0.01);border:1px solid var(--border)}"
            ".header{text-align:center;margin-bottom:20px}"
            ".header h2{font-size:22px;font-weight:700;color:var(--primary);display:flex;align-items:center;justify-content:center;gap:8px}"
            ".header p{font-size:13px;color:var(--subtext);margin-top:4px}"
            ".info-badge{background:#eff6ff;border:1px solid #bfdbfe;border-radius:8px;padding:10px 14px;font-size:12px;color:#1e40af;margin-bottom:20px;display:flex;justify-content:space-between;align-items:center}"
            ".section-title{font-size:14px;font-weight:600;color:var(--text);margin:16px 0 10px;text-transform:uppercase;letter-spacing:0.5px}"
            ".form-group{margin-bottom:14px}"
            "label{display:block;font-size:13px;font-weight:500;margin-bottom:6px;color:var(--text)}"
            "input,select{width:100%;padding:10px 14px;border:1px solid var(--border);border-radius:8px;font-size:14px;background:#fff;outline:none;transition:all .2s}"
            "input:focus,select:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(37,99,235,0.15)}"
            ".pwd-box{position:relative}"
            ".pwd-box span{position:absolute;right:12px;top:50%;transform:translateY(-50%);cursor:pointer;font-size:16px;user-select:none}"
            ".scan-btn{background:#f1f5f9;border:1px solid var(--border);color:var(--text);font-size:12px;padding:6px 12px;border-radius:6px;cursor:pointer;margin-left:auto;transition:.2s}"
            ".scan-btn:hover{background:#e2e8f0}"
            ".flex-between{display:flex;justify-content:space-between;align-items:center}"
            ".btn-primary{width:100%;padding:12px;background:var(--primary);color:#fff;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;margin-top:10px;transition:.2s}"
            ".btn-primary:hover{background:var(--primary-hover)}"
            ".actions{display:flex;gap:10px;margin-top:14px}"
            ".btn-sub{flex:1;padding:8px;background:#f8fafc;border:1px solid var(--border);border-radius:6px;font-size:12px;cursor:pointer;color:var(--subtext)}"
            ".btn-sub:hover{background:#f1f5f9;color:#0f172a}"
            ".footer{text-align:center;font-size:11px;color:var(--subtext);margin-top:18px}"
            "</style></head>"
            "<body>"
            "<div class='card'>"
            "<div class='header'>"
            "<h2>AIoT Platform</h2>"
            "<p>Cấu hình WiFi & Máy chủ HiveMQ Cloud</p>"
            "</div>"
            "<div class='info-badge'>"
            "<span><b>MAC:</b> ");
        html += mac;
        html += F(
            "</span>"
            "<span style='color:var(--success);font-weight:600;'>[AP Mode]</span>"
            "</div>"
            "<form method='POST' action='/save'>"
            "<div class='flex-between'>"
            "<div class='section-title'>1. Cấu hình WiFi</div>"
            "<button type='button' class='scan-btn' onclick='scanWiFi()'>Quét WiFi</button>"
            "</div>"
            "<div class='form-group'>"
            "<label for='ssid'>Tên WiFi (SSID):</label>"
            "<select id='wifi_list' onchange='selectSSID(this)' style='display:none;margin-bottom:8px;'></select>"
            "<input type='text' id='ssid' name='ssid' value='");
        html += curWifi;
        html += F(
            "' placeholder='Nhập hoặc chọn tên WiFi' required>"
            "</div>"
            "<div class='form-group'>"
            "<label for='pass'>Mật khẩu WiFi:</label>"
            "<div class='pwd-box'>"
            "<input type='password' id='pass' name='pass' placeholder='Nhập mật khẩu WiFi'>"
            "<span onclick='togglePwd(\"pass\")' style='font-size:11px;font-weight:600;'>[Hiện]</span>"
            "</div>"
            "</div>"
            "<div class='section-title' style='margin-top:18px;'>2. Máy chủ MQTT</div>"
            "<div class='info-badge' style='background:#f0fdf4;border-color:#bbf7d0;color:#166534;margin-bottom:12px;font-size:11px;'>"
            "<span>Máy chủ: <b>HiveMQ Cloud TLS (Port 8883)</b></span>"
            "</div>"
            "<div class='form-group'>"
            "<label for='mqtt_user'>Tài khoản (Tùy chọn):</label>"
            "<input type='text' id='mqtt_user' name='mqtt_user' value='");
        html += user;
        html += F(
            "' placeholder='Để trống nếu dùng mặc định'>"
            "</div>"
            "<div class='form-group'>"
            "<label for='mqtt_pass'>Mật khẩu MQTT (Tùy chọn):</label>"
            "<div class='pwd-box'>"
            "<input type='password' id='mqtt_pass' name='mqtt_pass' value='");
        html += pass;
        html += F(
            "' placeholder='Để trống nếu dùng mặc định'>"
            "<span onclick='togglePwd(\"mqtt_pass\")' style='font-size:11px;font-weight:600;'>[Hiện]</span>"
            "</div>"
            "</div>"
            "<button type='submit' class='btn-primary'>LƯU & KẾT NỐI NGAY</button>"
            "</form>"
            "<div class='actions'>"
            "<button class='btn-sub' onclick='doAction(\"/restart\")'>Khởi động lại</button>"
            "<button class='btn-sub' style='color:#ef4444;' onclick='if(confirm(\"Xóa toàn bộ cấu hình WiFi/MQTT đã lưu?\"))doAction(\"/reset\")'>Xóa cài đặt</button>"
            "</div>"
            "<div class='footer'>AIoT Platform - Multi-chip IoT Framework</div>"
            "</div>"
            "<script>"
            "function togglePwd(id){var x=document.getElementById(id);x.type=x.type==='password'?'text':'password';}"
            "function selectSSID(sel){if(sel.value){document.getElementById('ssid').value=sel.value;}}"
            "function scanWiFi(){"
            "var btn=event.target;btn.innerText='Đang quét...';btn.disabled=true;"
            "fetch('/scan').then(r=>r.json()).then(data=>{"
            "var sel=document.getElementById('wifi_list');sel.innerHTML='<option value=\"\">-- Chọn WiFi đã quét được --</option>';"
            "data.forEach(w=>{var opt=document.createElement('option');opt.value=w.ssid;opt.text=w.ssid+' ('+w.rssi+' dBm)';sel.appendChild(opt);});"
            "sel.style.display='block';btn.innerText='Quét lại';btn.disabled=false;"
            "}).catch(()=>{alert('Không thể quét WiFi!');btn.innerText='Quét WiFi';btn.disabled=false;});"
            "}"
            "function doAction(url){fetch(url,{method:'POST'}).then(()=>{alert('Đang thực hiện...');setTimeout(()=>location.reload(),3000);});}"
            "</script>"
            "</body></html>");

        return html;
    }

    static String MQTTConfigPage(
        const String &wifi,
        const String &user,
        const String &pass)
    {
        return ConfigPage(WiFi.macAddress(), wifi, user, pass);
    }
};

#endif
