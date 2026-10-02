#ifndef AIOT_PNP_ESP32
#define AIOT_PNP_ESP32
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

#include <MQTT/ESP32_MQTT.hpp>
#include <WiFi/CONFIG_UI.h>
#include <lwip/dns.h>
#include <esp_netif.h>

#define WIFI_AP_Subnet IPAddress(255, 255, 255, 0)
#define STA_WIFI_PORT "80"

#define AP_WIFI_NAME "AIoT: "
#ifndef AP_WIFI_PASS
#define AP_WIFI_PASS "IoT210605"
#endif
#define AP_WIFI_IP "192.168.21.6"
#define AP_WIFI_PORT "80"

enum WIFI_STATE
{
    MODE_STARTUP_STA,
    MODE_CONNECT_MQTT,
    MODE_CONNECTED,

    MODE_LOST_CONNECT_MQTT,
    MODE_LOST_CONNECT_WIFI,

    MODE_FAILD_CONNECT_WIFI,
    MODE_FAILD_CONNECT_MQTT,

    MODE_STARTUP_AP,
    MODE_CONFIG,
};

template <class Transport>
class PnP
{
    WebServer webServer{80};
    Preferences prefs;
    DNSServer dnsServer;

public:
    PnP() : WiFi_STATE(MODE_STARTUP_STA), _userConfiguring(false), _serverConfigured(false), _lastActivityTime(0) {};
    bool setupAndVerifyNetwork();
    // STARTUP_STATE
    void begin(const char *sta_ssid, const char *sta_pass);
    void begin(const char *sta_ssid, const char *sta_pass, const char *mqtt_id, const char *mqtt_auth);

    // LOOP STATE
    void run();

    // Co cau hinh tu nguoi dung
    void setUserConfiguring(bool configuring)
    {
        _userConfiguring = configuring;
        if (configuring)
            _lastActivityTime = millis();
    }
    bool isUserConfiguring() const { return _userConfiguring; }
    WIFI_STATE getState() const { return WiFi_STATE; }

    // SYSTEM Module
    void SaveWiFi(String newSSID, String newPASS);
    void SaveMQTT(String mqttID, String mqttAUTH);
    void loadWiFi();
    void loadMQTT();
    void resetCONFIGMODE();

    // RUN_STATE
    void CONFIG_STA();
    void CONFIG_MQTT();
    void CONNECTED();

    void RECONNECT_WIFI();
    void RECONNECT_MQTT();

    void FAILD_WIFI();
    void FAILD_MQTT();

    void CONFIG_AP();
    void RUN_AP_WEB();

    // CONFIG_MODULE
    void ConfigPage();
    void ConfigMQTTPage();
    void ConfigWiFiPage();
    void handleSaveWiFi();
    void handleSaveMQTT();
    void handleScanWiFi();

    // Handle_BUTTON_CONFIG
    void handler_button();

private:
    WIFI_STATE WiFi_STATE;
    bool _userConfiguring;
    bool _serverConfigured;
    unsigned long _lastActivityTime;

    IPAddress _ipAddr;
    // ===== STA ===== //
    char _sta_ssid[64];
    char _sta_pass[64];
    char _sta_ip[16];
    char _sta_port[5] = STA_WIFI_PORT;
    int _rssi;
    // ===== AP ===== //
    char _ap_ssid[64] = AP_WIFI_NAME;
    char _ap_pass[64] = AP_WIFI_PASS;
    char _ap_ip[16] = AP_WIFI_IP;
    char _ap_port[5] = AP_WIFI_PORT;
    char _mac[18];

    // ===== MQTT INIT ===== //
    char mqttusername[64];
    char mqttpass[64];
    // ===== MQTT NVS Flash ===== //
    char _mqtt_username[64];
    char _mqtt_pass[64];

    unsigned long t0, t1, t2;
    int time_STA = 8000;

#define Saved_WiFi_MAX 3
#define Scan_WiFi_MAX 10
    String newSSID;
    String newPASS;

    // ===== WIFI SAVED ===== //
    String saved_ssid[Saved_WiFi_MAX];
    String saved_pass[Saved_WiFi_MAX];

    // ===== WIFI SCAN ===== //
    String scan_ssid[Scan_WiFi_MAX];
    int scan_rssi[Scan_WiFi_MAX];

#define CONFIG_BTN 0
};

//======================================================
// Handle SYSTEM
//======================================================
template <class Transport>
inline void PnP<Transport>::SaveWiFi(String newSSID, String newPASS)
{
    if (newSSID.length() == 0)
        return;
    loadWiFi();
    if (!prefs.begin("wifi", false))
    {
        LOG_ERROR("WIFI", "NVS OPEN FAIL");
        return;
    }
    // =========================
    // CHECK EXIST SSID
    // =========================
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        if (saved_ssid[i] == newSSID)
        {
            if (saved_pass[i] == newPASS)
            {
                LOG_WIFI("WIFI", "SSID & PASS EXISTS -> SKIP");
                prefs.end();
                return;
            }
            LOG_WIFI("WIFI", "SSID EXISTS -> UPDATE PASS");
            for (int j = i; j > 0; j--)
            {
                saved_ssid[j] = saved_ssid[j - 1];
                saved_pass[j] = saved_pass[j - 1];
            }
            saved_ssid[0] = newSSID;
            saved_pass[0] = newPASS;
            for (int k = 0; k < Saved_WiFi_MAX; k++)
            {
                String keyS = "ssid" + String(k);
                String keyP = "pass" + String(k);
                if (saved_ssid[k].length() == 0)
                {
                    prefs.remove(keyS.c_str());
                    prefs.remove(keyP.c_str());
                }
                else
                {
                    prefs.putString(keyS.c_str(), saved_ssid[k]);
                    prefs.putString(keyP.c_str(), saved_pass[k]);
                }
            }
            prefs.end();
            LOG_WIFI("WIFI", "UPDATE WIFI DONE: %s", newSSID.c_str());
            return;
        }
    }
    // =========================
    // ADD NEW WIFI
    // =========================
    for (int i = Saved_WiFi_MAX - 1; i > 0; i--)
    {
        saved_ssid[i] = saved_ssid[i - 1];
        saved_pass[i] = saved_pass[i - 1];
    }
    saved_ssid[0] = newSSID;
    saved_pass[0] = newPASS;
    // SAVE NVS
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        String keyS = "ssid" + String(i);
        String keyP = "pass" + String(i);

        if (saved_ssid[i].length() == 0)
        {
            prefs.remove(keyS.c_str());
            prefs.remove(keyP.c_str());
        }
        else
        {
            prefs.putString(keyS.c_str(), saved_ssid[i]);
            prefs.putString(keyP.c_str(), saved_pass[i]);
        }
    }
    prefs.end();
    LOG_WIFI("WIFI", "SAVE NEW WIFI DONE: %s", newSSID.c_str());
}

template <class Transport>
inline void PnP<Transport>::SaveMQTT(String mqttuser, String mqttpass)
{
    if (mqttuser.length() == 0)
        return;
    loadMQTT();
    // =========================
    // USER & PASS GIONG -> SKIP
    // =========================
    if ((mqttuser == _mqtt_username) && (mqttpass == _mqtt_pass))
    {
        LOG_MQTT("MQTT", "MQTT EXISTS -> SKIP");
        return;
    }
    // =========================
    // USER GIONG - PASS KHAC
    // =========================
    if (mqttuser == _mqtt_username &&
        mqttpass != _mqtt_pass)
    {
        LOG_MQTT("MQTT", "MQTT USER EXISTS -> UPDATE PASS");
    }
    else
    {
        LOG_MQTT("MQTT", "SAVE NEW MQTT");
    }
    if (!prefs.begin("mqtt", false))
        return;
    prefs.putString("user", mqttuser);
    prefs.putString("pass", mqttpass);
    prefs.end();
    strncpy(_mqtt_username, mqttuser.c_str(), sizeof(_mqtt_username) - 1);
    _mqtt_username[sizeof(_mqtt_username) - 1] = '\0';
    strncpy(_mqtt_pass, mqttpass.c_str(), sizeof(_mqtt_pass) - 1);
    _mqtt_pass[sizeof(_mqtt_pass) - 1] = '\0';
    LOG_MQTT("MQTT", "SAVE MQTT DONE");
}

template <class Transport>
inline void PnP<Transport>::loadWiFi()
{
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        saved_ssid[i] = "";
        saved_pass[i] = "";
    }
    if (!prefs.begin("wifi", true))
        return;
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        String ssid = prefs.getString(("ssid" + String(i)).c_str(), "");
        String pass = prefs.getString(("pass" + String(i)).c_str(), "");
        if (ssid.length() > 0)
        {
            saved_ssid[i] = ssid;
            saved_pass[i] = pass;
        }
    }
    LOG_DEBUG("WIFI", "LOAD WiFi DONE");
    prefs.end();
}

template <class Transport>
inline void PnP<Transport>::loadMQTT()
{
    memset(_mqtt_username, 0, sizeof(_mqtt_username));
    memset(_mqtt_pass, 0, sizeof(_mqtt_pass));
    if (!prefs.begin("mqtt", true))
        return;
    String user = prefs.getString("user", "");
    String pass = prefs.getString("pass", "");
    prefs.end();
    strncpy(_mqtt_username, user.c_str(), sizeof(_mqtt_username) - 1);
    _mqtt_username[sizeof(_mqtt_username) - 1] = '\0';
    strncpy(_mqtt_pass, pass.c_str(), sizeof(_mqtt_pass) - 1);
    _mqtt_pass[sizeof(_mqtt_pass) - 1] = '\0';
    LOG_DEBUG("MQTT", "LOAD MQTT DONE");
}

template <class Transport>
inline void PnP<Transport>::resetCONFIGMODE()
{
}

template <class Transport>
inline void PnP<Transport>::handleSaveWiFi()
{
    _userConfiguring = true;
    _lastActivityTime = millis();

    String reqSSID = webServer.arg("ssid");
    String reqPASS = webServer.arg("pass");
    String mqttUser = webServer.arg("mqtt_user");
    String mqttPass = webServer.arg("mqtt_pass");

    if (reqSSID.length() > 0)
    {
        strncpy(_sta_ssid, reqSSID.c_str(), sizeof(_sta_ssid) - 1);
        _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
        strncpy(_sta_pass, reqPASS.c_str(), sizeof(_sta_pass) - 1);
        _sta_pass[sizeof(_sta_pass) - 1] = '\0';
        SaveWiFi(reqSSID, reqPASS);
    }
    if (mqttUser.length() > 0)
    {
        SaveMQTT(mqttUser, mqttPass);
    }

    String resp = F("<!DOCTYPE html><html lang='vi'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
                    "<title>Đã Lưu Cấu Hình</title><style>"
                    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f8fafc;color:#1e293b;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:16px;margin:0;}"
                    ".card{background:#fff;border-radius:12px;padding:30px;max-width:380px;text-align:center;box-shadow:0 10px 25px rgba(0,0,0,0.05);border:1px solid #e2e8f0;}"
                    "h2{color:#10b981;font-size:20px;margin-bottom:12px;}"
                    "p{font-size:14px;color:#64748b;line-height:1.5;margin-bottom:8px;}"
                    ".note{font-size:12px;color:#94a3b8;margin-top:16px;}"
                    "</style></head><body>"
                    "<div class='card'>"
                    "<h2>&#10004; Lưu Cấu Hình Thành Công!</h2>"
                    "<p>Thiết bị đang chuyển sang chế độ WiFi và kết nối máy chủ...</p>"
                    "<p class='note'>Bạn có thể ngắt kết nối với AP này.</p>"
                    "</div></body></html>");
    webServer.send(200, "text/html", resp);
    delay(1000);
    webServer.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _userConfiguring = false;
    WiFi_STATE = MODE_STARTUP_STA;
}

template <class Transport>
inline void PnP<Transport>::handleSaveMQTT()
{
    _userConfiguring = true;
    _lastActivityTime = millis();
    String mqttUser = webServer.arg("mqtt_user");
    String mqttPass = webServer.arg("mqtt_pass");
    SaveMQTT(mqttUser, mqttPass);
    serverMQTT.disconnect();
    WiFi_STATE = MODE_CONNECT_MQTT;
}

template <class Transport>
inline void PnP<Transport>::handleScanWiFi()
{
    _userConfiguring = true;
    _lastActivityTime = millis();

    WiFi.scanDelete();
    int n = WiFi.scanNetworks();
    String json = "[";
    if (n > 0)
    {
        int count = min(n, Scan_WiFi_MAX);
        bool first = true;
        for (int i = 0; i < count; i++)
        {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0)
                continue;
            ssid.replace("\\", "\\\\");
            ssid.replace("\"", "\\\"");

            if (!first)
                json += ",";
            json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
            first = false;
        }
    }
    json += "]";
    WiFi.scanDelete();
    webServer.send(200, "application/json", json);
}

template <class Transport>
inline void PnP<Transport>::ConfigPage()
{
    _userConfiguring = true;
    _lastActivityTime = millis();
    loadWiFi();
    loadMQTT();
    String curSSID = (saved_ssid[0].length() > 0) ? saved_ssid[0] : String(_sta_ssid);
    String html = WebUI::ConfigPage(String(_mac), curSSID, String(_mqtt_username), String(_mqtt_pass));
    webServer.send(200, "text/html", html);
}

template <class Transport>
inline void PnP<Transport>::ConfigMQTTPage()
{
    ConfigPage();
}

template <class Transport>
inline void PnP<Transport>::ConfigWiFiPage()
{
    ConfigPage();
}

//======================================================
// STARTUP CONFIG SYSTEM
//======================================================
template <class Transport>
inline void PnP<Transport>::begin(const char *sta_ssid, const char *sta_pass)
{
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    delay(200);
    strncpy(_sta_ssid, sta_ssid, sizeof(_sta_ssid) - 1);
    _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
    strncpy(_sta_pass, sta_pass, sizeof(_sta_pass) - 1);
    _sta_pass[sizeof(_sta_pass) - 1] = '\0';
    String MAC = WiFi.macAddress();
    strncpy(_mac, MAC.c_str(), sizeof(_mac) - 1);
    _mac[sizeof(_mac) - 1] = '\0';
    snprintf(_ap_ssid, sizeof(_ap_ssid), "%s%s", AP_WIFI_NAME, _mac);
    LOG_WIFI("WIFI", "STA_WIFI_NAME: %s", _sta_ssid);
    LOG_WIFI("WIFI", "STA_WIFI_PASS: %s", _sta_pass);
    LOG_WIFI("WIFI", "STA_WIFI_IP: %s", _sta_ip);
    LOG_WIFI("WIFI", "STA_WIFI_PORT: %s", _sta_port);
    LOG_DEBUG("WIFI", "STARTING CONFIG");
}
template <class Transport>
inline void PnP<Transport>::begin(const char *sta_ssid, const char *sta_pass, const char *mqtt_username, const char *mqtt_pass)
{
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    delay(200);
    strncpy(_sta_ssid, sta_ssid, sizeof(_sta_ssid) - 1);
    _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
    strncpy(_sta_pass, sta_pass, sizeof(_sta_pass) - 1);
    _sta_pass[sizeof(_sta_pass) - 1] = '\0';
    strncpy(mqttusername, mqtt_username, sizeof(mqttusername) - 1);
    mqttusername[sizeof(mqttusername) - 1] = '\0';
    strncpy(mqttpass, mqtt_pass, sizeof(mqttpass) - 1);
    mqttpass[sizeof(mqttpass) - 1] = '\0';
    String MAC = WiFi.macAddress();
    strncpy(_mac, MAC.c_str(), sizeof(_mac) - 1);
    _mac[sizeof(_mac) - 1] = '\0';
    snprintf(_ap_ssid, sizeof(_ap_ssid), "%s%s", AP_WIFI_NAME, _mac);
    LOG_WIFI("WIFI", "STA_WIFI_NAME: %s", _sta_ssid);
    LOG_WIFI("WIFI", "STA_WIFI_PASS: %s", _sta_pass);
    LOG_WIFI("WIFI", "STA_WIFI_IP: %s", _sta_ip);
    LOG_WIFI("WIFI", "STA_WIFI_PORT: %s", _sta_port);
    LOG_DEBUG("WIFI", "STARTING CONFIG");
}

//======================================================
// NETWORK SETUP & DNS VERIFICATION
//======================================================
template <class Transport>
inline bool PnP<Transport>::setupAndVerifyNetwork()
{
    // 1. Cho IP va Gateway tu DHCP
    unsigned long t_ip = millis();
    while ((WiFi.localIP() == IPAddress(0, 0, 0, 0) || WiFi.gatewayIP() == IPAddress(0, 0, 0, 0)) && millis() - t_ip < 4000)
    {
        delay(50);
    }

    if (WiFi.localIP() == IPAddress(0, 0, 0, 0))
    {
        LOG_ERROR("WIFI", "DHCP FAILED TO OBTAIN IP ADDRESS");
        return false;
    }

    // 2. Cau hinh Backup DNS (8.8.8.8 & 1.1.1.1) neu mang chua co DNS tu DHCP
    if (WiFi.dnsIP(0) == IPAddress(0, 0, 0, 0))
    {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif)
        {
            esp_netif_dns_info_t dns;
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(IPAddress(8, 8, 8, 8));
            esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
            dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(IPAddress(1, 1, 1, 1));
            esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns);
        }
        ip_addr_t d1, d2;
        d1.type = IPADDR_TYPE_V4;
        d1.u_addr.ip4.addr = static_cast<uint32_t>(IPAddress(8, 8, 8, 8));
        dns_setserver(0, &d1);
        d2.type = IPADDR_TYPE_V4;
        d2.u_addr.ip4.addr = static_cast<uint32_t>(IPAddress(1, 1, 1, 1));
        dns_setserver(1, &d2);
    }

    // 3. Xac thuc DNS hoat dong
    IPAddress resolved;
    bool dnsOk = false;
    unsigned long t_dns = millis();
    while (!dnsOk && millis() - t_dns < 3000)
    {
        if (WiFi.hostByName("google.com", resolved) == 1 ||
            WiFi.hostByName("cloudflare.com", resolved) == 1)
        {
            dnsOk = true;
            break;
        }
        delay(100);
    }

    LOG_WIFI("WIFI", "NETWORK READY: IP=%s, Gateway=%s, DNS1=%s, DNS2=%s (DNS: %s)",
             WiFi.localIP().toString().c_str(),
             WiFi.gatewayIP().toString().c_str(),
             WiFi.dnsIP(0).toString().c_str(),
             WiFi.dnsIP(1).toString().c_str(),
             dnsOk ? "OK" : "TIMEOUT");

    return true;
}

//======================================================
// STA RUNNING
//======================================================
template <class Transport>
inline void PnP<Transport>::CONFIG_STA()
{
    webServer.stop();
    dnsServer.stop();
    loadWiFi();
    loadMQTT();
    // =========================
    // 1. UU TIEN WIFI INIT
    // =========================
    if (strlen(_sta_ssid) > 0)
    {
        WiFi.disconnect();
        delay(200);
        t1 = millis();
        t0 = millis();
        WiFi.begin(_sta_ssid, _sta_pass);
        LOG_WIFI("WIFI", "CONNECT WIFI WITH: %s", _sta_ssid);
        while (WiFi.status() != WL_CONNECTED && millis() - t1 <= time_STA)
        {
            if (millis() - t0 >= 1000)
            {
                LOG_WIFI("WIFI", "CONNECTING....... %ds", (millis() - t1) / 1000);
                t0 = millis();
            }
            delay(10);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            if (setupAndVerifyNetwork())
            {
                SaveWiFi(_sta_ssid, _sta_pass);
                LOG_WIFI("WIFI", "WiFi SIGNAL STRENGTH: %s", (String(WiFi.RSSI()) + "dBm").c_str());
                WiFi_STATE = MODE_CONNECT_MQTT;
                delay(100);
                return;
            }
        }
    }
    // ========================================
    // 2. KET NOI VOI WIFI DA LUU TRONG BO NHO
    // ========================================
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        if (saved_ssid[i].length() == 0)
            continue;
        LOG_WIFI("WIFI", "FOUND MATCH: %s", saved_ssid[i].c_str());
        WiFi.disconnect();
        delay(100);
        t1 = millis();
        t0 = millis();
        WiFi.begin(saved_ssid[i].c_str(), saved_pass[i].c_str());
        LOG_WIFI("WIFI", "CONNECT WIFI WITH: %s", saved_ssid[i].c_str());
        while (WiFi.status() != WL_CONNECTED && millis() - t1 <= time_STA)
        {
            if (millis() - t0 >= 1000)
            {
                LOG_WIFI("WIFI", "CONNECTING....... %ds", (millis() - t1) / 1000);
                t0 = millis();
            }
            delay(10);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            strncpy(_sta_ssid, saved_ssid[i].c_str(), sizeof(_sta_ssid) - 1);
            _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
            strncpy(_sta_pass, saved_pass[i].c_str(), sizeof(_sta_pass) - 1);
            _sta_pass[sizeof(_sta_pass) - 1] = '\0';

            if (setupAndVerifyNetwork())
            {
                SaveWiFi(saved_ssid[i].c_str(), saved_pass[i].c_str());
                LOG_WIFI("WIFI", "WiFi signal strength: %s", (String(WiFi.RSSI()) + "dBm").c_str());
                WiFi_STATE = MODE_CONNECT_MQTT;
                delay(100);
                return;
            }
        }
    }
    // =====================================
    // 3. KHONG THE KET NOI WIFI -> AP MODE
    // =====================================
    LOG_WIFI("WIFI", "NO WIFI CONNECTED -> AP MODE");
    WiFi.disconnect();
    mqttClient.disconnect();
    WiFi_STATE = MODE_STARTUP_AP;
    delay(100);
}

// CONFIGURING MQTT
template <class Transport>
inline void PnP<Transport>::CONFIG_MQTT()
{
    LOG_WIFI("WIFI", "WIFI_CONNECTED!!!");
    LOG_WIFI("WIFI", "STA_WIFI_NAME: %s", _sta_ssid);
    LOG_WIFI("WIFI", "STA_DEVICE_IP: %s", WiFi.localIP().toString().c_str());
    LOG_WIFI("WIFI", "DEVICE_MAC: %s", _mac);
    if (strlen(_mqtt_username) > 0)
    {
        LOG_MQTT("MQTT", "USE NVS MQTT");
        serverMQTT.config(_mqtt_username, _mqtt_pass);
    }
    else
    {
        LOG_MQTT("MQTT", "EEPROM EMPTY -> USE INIT MQTT");
        serverMQTT.config(mqttusername, mqttpass);
    }
    serverMQTT.begin();
    if (WiFi.status() != WL_CONNECTED)
    {
        LOG_ERROR("WIFI", "LOST CONNECT TO WIFI");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
        delay(500);
        return;
    }
    if (serverMQTT.check_connect())
    {
        if (Serial)
        {
            Serial.print(
                "\r\n"
                "            █████╗ ██╗          ████████╗\r\n"
                "           ██╔══██╗██║          ╚══██╔══╝\r\n"
                "           ███████║██║  ██████╗    ██║   \r\n"
                "           ██╔══██║██║ ██╔═══██╗   ██║   \r\n"
                "           ██║  ██║██║ ╚██████╔╝   ██║   \r\n"
                "           ╚═╝  ╚═╝╚═╝  ╚═════╝    ╚═╝   \r\n"
                "           AIoT Firmware v1.0.0\r\n");
            Serial.println("===============================================================");
            Serial.println("       ESP32-S3 HYBRID AIoT CHAT SYSTEM SẴN SÀNG!");
            Serial.println("       - Gõ 'xin chào' để kích hoạt Greeting_action & mở đầu");
            Serial.println("       - Gõ bất kỳ câu hỏi nào để trò chuyện cùng AI");
            Serial.println("=================================================================");
        }
        WiFi_STATE = MODE_CONNECTED;
        delay(500);
        return;
    }
    else
    {
        WiFi_STATE = MODE_FAILD_CONNECT_MQTT;
        delay(500);
        return;
    }
}

template <class Transport>
inline void PnP<Transport>::CONNECTED()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        LOG_ERROR("WIFI", "LOST CONNECT TO WIFI");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
        return;
    }

    if (serverMQTT.check_connect())
    {
        serverMQTT.run();
    }
    else
    {
        LOG_ERROR("MQTT", "LOST CONNECT TO MQTT (state=%d)", serverMQTT.getState());
        LOG_ERROR("MQTT", "TRY RECONNECT TO MQTT");
        WiFi_STATE = MODE_LOST_CONNECT_MQTT;
        delay(500);
    }
}

//======================================================
// AUTO FIX RUNNING
//======================================================
template <class Transport>
inline void PnP<Transport>::RECONNECT_WIFI()
{
    LOG_WIFI("WIFI", "RECONNECTING WIFI...");

    if (strlen(_sta_ssid) == 0)
    {
        loadWiFi();
        if (saved_ssid[0].length() > 0)
        {
            strncpy(_sta_ssid, saved_ssid[0].c_str(), sizeof(_sta_ssid) - 1);
            _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
            strncpy(_sta_pass, saved_pass[0].c_str(), sizeof(_sta_pass) - 1);
            _sta_pass[sizeof(_sta_pass) - 1] = '\0';
        }
    }

    if (strlen(_sta_ssid) == 0 && saved_ssid[0].length() == 0)
    {
        LOG_ERROR("WIFI", "NO SAVED WIFI FOUND! SWITCHING TO AP MODE...");
        WiFi_STATE = MODE_STARTUP_AP;
        return;
    }

    dnsServer.stop();
    webServer.stop();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    // =========================
    // 1. THU WIFI HIEN TAI
    // =========================
    if (strlen(_sta_ssid) > 0)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            WiFi.begin(_sta_ssid, _sta_pass);
        }
        LOG_WIFI("WIFI", "RECONNECT WIFI WITH: %s", _sta_ssid);
        unsigned long t_start = millis();
        unsigned long t_log = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t_start < 8000)
        {
            if (millis() - t_log > 1000)
            {
                LOG_WIFI("WIFI", "RECONNECTING... %ds", (millis() - t_start) / 1000);
                t_log = millis();
            }
            delay(50);
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            if (setupAndVerifyNetwork())
            {
                LOG_WIFI("WIFI", "RECONNECT WIFI DONE (IP: %s)", WiFi.localIP().toString().c_str());
                WiFi_STATE = MODE_CONNECT_MQTT;
                return;
            }
        }
    }

    // =========================
    // 2. FALLBACK WIFI TRONG BO NHO
    // =========================
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        if (saved_ssid[i].length() == 0 || saved_ssid[i] == _sta_ssid)
            continue;
        WiFi.begin(saved_ssid[i].c_str(), saved_pass[i].c_str());
        LOG_WIFI("WIFI", "RECONNECT WIFI WITH: %s", saved_ssid[i].c_str());
        unsigned long t_start = millis();
        unsigned long t_log = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t_start < 8000)
        {
            if (millis() - t_log > 1000)
            {
                LOG_WIFI("WIFI", "RECONNECTING... %ds", (millis() - t_start) / 1000);
                t_log = millis();
            }
            delay(50);
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            strncpy(_sta_ssid, saved_ssid[i].c_str(), sizeof(_sta_ssid) - 1);
            _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
            strncpy(_sta_pass, saved_pass[i].c_str(), sizeof(_sta_pass) - 1);
            _sta_pass[sizeof(_sta_pass) - 1] = '\0';
            if (setupAndVerifyNetwork())
            {
                LOG_WIFI("WIFI", "RECONNECT WIFI DONE (IP: %s)", WiFi.localIP().toString().c_str());
                WiFi_STATE = MODE_CONNECT_MQTT;
                return;
            }
        }
    }

    // =========================
    // 3. FAIL
    // =========================
    LOG_ERROR("WIFI", "RECONNECT WIFI FAILED - WILL RETRY IN 5 SECONDS");
    WiFi_STATE = MODE_FAILD_CONNECT_WIFI;
}

template <class Transport>
inline void PnP<Transport>::RECONNECT_MQTT()
{
    LOG_MQTT("MQTT", "RECONNECT MQTT...");
    if (WiFi.status() != WL_CONNECTED)
    {
        LOG_ERROR("MQTT", "NO WIFI -> SWITCH TO WIFI RECOVERY");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
        return;
    }

    if (strlen(_mqtt_username) > 0)
    {
        serverMQTT.config(_mqtt_username, _mqtt_pass);
    }
    else
    {
        serverMQTT.config(mqttusername, mqttpass);
    }

    serverMQTT.begin();

    if (serverMQTT.check_connect())
    {
        LOG_MQTT("MQTT", "MQTT RECONNECTED OK");
        WiFi_STATE = MODE_CONNECTED;
        return;
    }
    LOG_ERROR("MQTT", "RECONNECT FAILED");
    WiFi_STATE = MODE_FAILD_CONNECT_MQTT;
}

//======================================================
// CONFIG FIX RUNNING
//======================================================
template <class Transport>
inline void PnP<Transport>::FAILD_MQTT()
{
    static unsigned long lastMqttRetry = 0;
    static bool mqttRetryTimerStarted = false;
    if (!mqttRetryTimerStarted)
    {
        lastMqttRetry = millis();
        mqttRetryTimerStarted = true;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        mqttRetryTimerStarted = false;
        LOG_ERROR("MQTT", "WIFI LOST WHILE RECOVERING MQTT -> SWITCH TO WIFI RECOVERY");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
        return;
    }

    if ((unsigned long)(millis() - lastMqttRetry) >= 5000UL)
    {
        mqttRetryTimerStarted = false;
        LOG_MQTT("MQTT", "RETRYING MQTT CONNECTION...");
        WiFi_STATE = MODE_LOST_CONNECT_MQTT;
    }
}

template <class Transport>
inline void PnP<Transport>::FAILD_WIFI()
{
    static unsigned long lastFailTime = 0;
    static bool failWifiTimerStarted = false;
    if (!failWifiTimerStarted)
    {
        lastFailTime = millis();
        failWifiTimerStarted = true;
    }
    if ((unsigned long)(millis() - lastFailTime) >= 5000UL)
    {
        failWifiTimerStarted = false;
        LOG_WIFI("WIFI", "RETRYING STA CONNECTION...");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
    }
}

//======================================================
// AP RUNNING
//======================================================
template <class Transport>
inline void PnP<Transport>::CONFIG_AP()
{
    webServer.stop();
    dnsServer.stop();
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    loadWiFi(); // load saved WiFi
    loadMQTT(); // load user&pass MQTT
    LOG_WIFI("AP", "RUN_AP");
    LOG_WIFI("AP", "AP_WIFI_NAME: %s", _ap_ssid);
    LOG_WIFI("AP", "AP_WIFI_PASS: %s", _ap_pass);
    LOG_WIFI("AP", "AP_WIFI_IP: %s", _ap_ip);
    LOG_WIFI("AP", "AP_WIFI_PORT: %s", _ap_port);
    IPAddress local_ip;
    local_ip.fromString(_ap_ip);
    WiFi.softAPConfig(local_ip, local_ip, WIFI_AP_Subnet);
    WiFi.softAP(_ap_ssid, _ap_pass);

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", local_ip);

    if (!_serverConfigured)
    {
        webServer.on("/", HTTP_GET, [this]()
                     { this->ConfigPage(); });
        webServer.on("/scan", HTTP_GET, [this]()
                     { this->handleScanWiFi(); });
        webServer.on("/save", HTTP_POST, [this]()
                     { this->handleSaveWiFi(); });
        webServer.on("/restart", HTTP_POST, [this]()
                     {
            webServer.send(200, "text/plain", "RESTARTING");
            delay(500);
            ESP.restart(); });
        webServer.on("/reset", HTTP_POST, [this]()
                     {
            webServer.send(200, "text/plain", "RESETTING");
            delay(500);
            prefs.begin("wifi", false);
            prefs.clear();
            prefs.end();
            prefs.begin("mqtt", false);
            prefs.clear();
            prefs.end();
            WiFi.disconnect(true, true);
            ESP.restart(); });
        webServer.on("/start_config", HTTP_GET, [this]()
                     {
            _userConfiguring = true;
            _lastActivityTime = millis();
            webServer.send(200, "text/plain", "OK"); });
        // Captive Portal redirection
        webServer.on("/generate_204", HTTP_GET, [this]()
                     { this->ConfigPage(); });
        webServer.on("/hotspot-detect.html", HTTP_GET, [this]()
                     { this->ConfigPage(); });
        webServer.on("/canonical.html", HTTP_GET, [this]()
                     { this->ConfigPage(); });
        webServer.onNotFound([this]()
                             {
            String host = webServer.hostHeader();
            if (host.length() > 0 && host != _ap_ip)
            {
                webServer.sendHeader("Location", String("http://") + _ap_ip + "/", true);
                webServer.send(302, "text/plain", "");
                return;
            }
            this->ConfigPage(); });

        _serverConfigured = true;
    }

    webServer.begin();
    LOG_WIFI("AP", "WEB CONFIG READY AT http://%s", _ap_ip);
    WiFi_STATE = MODE_CONFIG;
}

//================ BUTTON ================//
template <class Transport>
inline void PnP<Transport>::handler_button()
{
#ifdef BUTTON_CONFIG
    // Cau hinh nut nhan neu can
#endif
}

//======================================================
// SYSTEM RUNNING
//======================================================
template <class Transport>
inline void PnP<Transport>::run()
{
    switch (WiFi_STATE)
    {
    case MODE_STARTUP_STA:
        this->CONFIG_STA();
        break;

    case MODE_CONNECT_MQTT:
        this->CONFIG_MQTT();
        break;

    case MODE_CONNECTED:
        this->CONNECTED();
        break;

    case MODE_FAILD_CONNECT_WIFI:
        this->FAILD_WIFI();
        break;

    case MODE_FAILD_CONNECT_MQTT:
        this->FAILD_MQTT();
        break;

    case MODE_LOST_CONNECT_WIFI:
        this->RECONNECT_WIFI();
        break;

    case MODE_LOST_CONNECT_MQTT:
        this->RECONNECT_MQTT();
        break;

    case MODE_STARTUP_AP:
        this->CONFIG_AP();
        break;

    case MODE_CONFIG:
        dnsServer.processNextRequest();
        webServer.handleClient();

        // 1. Tu dong bat co neu co bat ky thiet bi nao ket noi vao SoftAP
        if (WiFi.softAPgetStationNum() > 0)
        {
            if (!_userConfiguring)
            {
                LOG_WIFI("AP", "STATION DETECTED ON AP -> USER CONFIGURING FLAG ACTIVATED!");
                _userConfiguring = true;
            }
            _lastActivityTime = millis();
        }

        // 2. Tu dong ha co neu khong con ai ket noi AP va khong co thao tac web trong 3 phut
        if (_userConfiguring && WiFi.softAPgetStationNum() == 0 && (millis() - _lastActivityTime > 180000UL))
        {
            LOG_WIFI("AP", "USER INACTIVE FOR 3 MINS -> RESET CONFIG FLAG");
            _userConfiguring = false;
        }

        // 3. Neu DANG CO CO (Nguoi dung dang cau hinh):
        // KHONG KET NOI NGAM, giu nguyen kenh phat cua AP de song on dinh cho nguoi dung thao tac!
        if (_userConfiguring)
        {
            break;
        }

        // 4. Neu CHUA CO CO (Chua ai vao cau hinh):
        // Dinh ky moi 15 giay thu ket noi ngam voi cac WiFi cu trong NVS
        {
            static unsigned long lastAPRetry = 0;
            if ((unsigned long)(millis() - lastAPRetry) > 15000UL)
            {
                lastAPRetry = millis();
                loadWiFi();

                bool hasKnown = (strlen(_sta_ssid) > 0) || (saved_ssid[0].length() > 0);
                if (hasKnown)
                {
                    for (int i = 0; i < Saved_WiFi_MAX; i++)
                    {
                        String targetSSID = (i == 0 && strlen(_sta_ssid) > 0) ? String(_sta_ssid) : saved_ssid[i];
                        String targetPass = (i == 0 && strlen(_sta_ssid) > 0) ? String(_sta_pass) : saved_pass[i];

                        if (targetSSID.length() == 0)
                            continue;

                        // Neu phat hien nguoi dung vua ket noi AP -> DUNG NGAY
                        if (WiFi.softAPgetStationNum() > 0 || _userConfiguring)
                        {
                            _userConfiguring = true;
                            _lastActivityTime = millis();
                            break;
                        }

                        LOG_WIFI("AP", "BACKGROUND RETRYING KNOWN WIFI: '%s'...", targetSSID.c_str());
                        WiFi.begin(targetSSID.c_str(), targetPass.c_str());

                        unsigned long checkStart = millis();
                        while (WiFi.status() != WL_CONNECTED && millis() - checkStart < 5000)
                        {
                            dnsServer.processNextRequest();
                            webServer.handleClient();

                            // Bat duoc nguoi dung vua ket noi AP hoac mo web -> NGAT THU NGAM LAP TUC!
                            if (WiFi.softAPgetStationNum() > 0 || _userConfiguring)
                            {
                                WiFi.disconnect();
                                _userConfiguring = true;
                                _lastActivityTime = millis();
                                LOG_WIFI("AP", "ABORT BACKGROUND RETRY: USER CONNECTED!");
                                break;
                            }
                            delay(50);
                        }

                        if (WiFi.status() == WL_CONNECTED)
                        {
                            LOG_WIFI("AP", "RECONNECTED TO '%s'! CLOSING AP MODE...", targetSSID.c_str());
                            webServer.stop();
                            dnsServer.stop();
                            WiFi.softAPdisconnect(true);
                            WiFi.mode(WIFI_STA);
                            strncpy(_sta_ssid, targetSSID.c_str(), sizeof(_sta_ssid) - 1);
                            _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
                            strncpy(_sta_pass, targetPass.c_str(), sizeof(_sta_pass) - 1);
                            _sta_pass[sizeof(_sta_pass) - 1] = '\0';
                            setupAndVerifyNetwork();
                            _userConfiguring = false;
                            WiFi_STATE = MODE_CONNECT_MQTT;
                            return;
                        }

                        // Neu co bat thi dung duyet
                        if (_userConfiguring)
                            break;
                    }
                }
            }
        }
        break;

    default:
        break;
    }
}

#endif
