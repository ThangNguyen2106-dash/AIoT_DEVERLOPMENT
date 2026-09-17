#ifndef AIOT_PNP_ESP32
#define AIOT_PNP_ESP32
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

#include <IoT/API.hpp>
#include <MQTT/ESP32_MQTT.hpp>
#include <WiFi/CONFIG_UI.h>

#define WIFI_AP_Subnet IPAddress(255, 255, 255, 0)
char STA_WIFI_NAME[32];
char STA_WIFI_PASS[32];
#define STA_WIFI_PORT "80"

#define AP_WIFI_NAME "AIoT: "
#define AP_WIFI_PASS "IoT210605"
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

WIFI_STATE WiFi_STATE = MODE_STARTUP_STA;
template <class Transport>
class PnP
{
    WebServer webServer{80};
    Preferences prefs;
    DNSServer dnsServer;

public:
    PnP() {};
    // STARTUP_STATE
    void begin(const char *sta_ssid, const char *sta_pass);
    void begin(const char *sta_ssid, const char *sta_pass, const char *mqtt_id, const char *mqtt_auth);

    // LOOP STATE
    void run();

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
    static const char WebConfigHEAD[] PROGMEM;
    static const char WebConfigFOOT[] PROGMEM;
    void ConfigPage();
    void ConfigMQTTPage();
    void ConfigWiFiPage();
    void handleSaveWiFi();
    void handleSaveMQTT();
    void handleScanWiFi();

    // Handle_BUTTON_CONFIG
    void handler_button();

private:
    IPAddress _ipAddr;
    // ===== STA ===== //
    char _sta_ssid[64];
    char _sta_pass[32];
    char _sta_ip[16];
    char _sta_port[5] = STA_WIFI_PORT;
    int _rssi;
    // ===== AP ===== //
    char _ap_ssid[64] = AP_WIFI_NAME;
    char _ap_pass[32] = AP_WIFI_PASS;
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
    int time_STA = 20000;

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
    // USER & PASS GIỐNG -> SKIP
    // =========================
    if ((mqttuser == _mqtt_username) && (mqttpass == _mqtt_pass))
    {
        LOG_MQTT("MQTT", "MQTT EXISTS -> SKIP");
        return;
    }
    // =========================
    // USER GIỐNG - PASS KHÁC
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
    strcpy(_mqtt_username, mqttuser.c_str());
    strcpy(_mqtt_pass, mqttpass.c_str());
    LOG_MQTT("MQTT", "SAVE MQTT DONE");
}

template <class Transport>
inline void PnP<Transport>::loadWiFi()
{
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
        else
        {
            saved_ssid[i] = "";
            saved_pass[i] = "";
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
    strcpy(_mqtt_username, user.c_str());
    strcpy(_mqtt_pass, pass.c_str());
    LOG_DEBUG("MQTT", "LOAD MQTT DONE");
}

template <class Transport>
inline void PnP<Transport>::resetCONFIGMODE()
{
}

template <class Transport>
inline void PnP<Transport>::handleSaveWiFi()
{
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

    webServer.send(200, "text/plain", "OK");
    delay(500);
    webServer.stop();
    dnsServer.stop();
    WiFi.mode(WIFI_STA);
    WiFi_STATE = MODE_STARTUP_STA;
}

template <class Transport>
inline void PnP<Transport>::handleSaveMQTT()
{
    String mqttUser = webServer.arg("mqtt_user");
    String mqttPass = webServer.arg("mqtt_pass");
    SaveMQTT(mqttUser, mqttPass);
    serverMQTT.disconnect();
    WiFi_STATE = MODE_CONNECT_MQTT;
}

template <class Transport>
inline void PnP<Transport>::handleScanWiFi()
{
    WiFi.scanDelete();
    int n = WiFi.scanNetworks();
    String json = "[";
    if (n > 0)
    {
        int count = min(n, Scan_WiFi_MAX);
        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
    }
    json += "]";
    WiFi.scanDelete();
    webServer.send(200, "application/json", json);
}

template <class Transport>
inline void PnP<Transport>::ConfigPage()
{
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
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    delay(500);
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
    pinMode(CONFIG_BTN, INPUT_PULLUP);
}
template <class Transport>
inline void PnP<Transport>::begin(const char *sta_ssid, const char *sta_pass, const char *mqtt_username, const char *mqtt_pass)
{
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    delay(500);
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
    pinMode(CONFIG_BTN, INPUT_PULLUP);
}

//======================================================
// STA RUNNING
//======================================================
template <class Transport>
inline void PnP<Transport>::CONFIG_STA()
{
    webServer.stop();
    loadWiFi();
    loadMQTT();
    // =========================
    // 1. ƯU TIÊN WIFI INIT
    // =========================
    if (strlen(_sta_ssid) > 0)
    {
        t1 = millis();
        WiFi.disconnect();
        delay(200);
        t1 = millis();
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
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            unsigned long t_ip = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t_ip < 3000)
            {
                delay(50);
            }
            SaveWiFi(_sta_ssid, _sta_pass);
            LOG_WIFI("WIFI", "WiFi SIGNAL STRENGTH: %s", (String(WiFi.RSSI()) + "dBm").c_str());
            // Serial.println(WiFi.RSSI());
            WiFi_STATE = MODE_CONNECT_MQTT;
            delay(100);
            return;
        }
    }
    // ========================================
    // 2. KẾT NỐI VỚI WIFI ĐÃ LƯU TRONG BỘ NHỚ
    // ========================================
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        if (saved_ssid[i].length() == 0)
            continue;
        LOG_WIFI("WIFI", "FOUND MATCH: %s", saved_ssid[i].c_str());
        t1 = millis();
        WiFi.begin(saved_ssid[i].c_str(), saved_pass[i].c_str());
        LOG_WIFI("WIFI", "CONNECT WIFI WITH: %s", saved_ssid[i].c_str());
        while (WiFi.status() != WL_CONNECTED && millis() - t1 <= time_STA)
        {
            if (millis() - t0 > 1000)
            {
                LOG_WIFI("WIFI", "CONNECTING....... %ds", (millis() - t1) / 1000);
                t0 = millis();
            }
            delay(10);
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            unsigned long t_ip = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t_ip < 3000)
            {
                delay(50);
            }
            // Lưu lại SSID hiện tại vào bộ nhớ tạm để phục vụ Reconnect sau này
            strncpy(_sta_ssid, saved_ssid[i].c_str(), sizeof(_sta_ssid) - 1);
            _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
            strncpy(_sta_pass, saved_pass[i].c_str(), sizeof(_sta_pass) - 1);
            _sta_pass[sizeof(_sta_pass) - 1] = '\0';

            SaveWiFi(saved_ssid[i].c_str(), saved_pass[i].c_str());
            LOG_WIFI("WIFI", "WiFi signal strength: %s", (String(WiFi.RSSI()) + "dBm").c_str());
            WiFi_STATE = MODE_CONNECT_MQTT;
            delay(100);
            return;
        }
    }
    // =====================================
    // 3. KHÔNG THỂ KẾT NỐI WIFI -> AP MODE
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
        delay(1000);
    }
    if (serverMQTT.check_connect())
    {
        if (Serial)
        {
            Serial.print(
                "\r\n"
                " █████╗ ██╗          ████████╗\r\n"
                "██╔══██╗██║          ╚══██╔══╝\r\n"
                "███████║██║  ██████╗    ██║   \r\n"
                "██╔══██║██║ ██╔═══██╗   ██║   \r\n"
                "██║  ██║██║ ╚██████╔╝   ██║   \r\n"
                "╚═╝  ╚═╝╚═╝  ╚═════╝    ╚═╝   \r\n"
                "  AIoT Firmware v1.0.0\r\n"
                "  ESP32 AIoT Controller\r\n\r\n");
        }
        SaveMQTT(mqttusername, mqttpass);
        WiFi_STATE = MODE_CONNECTED;
        delay(1000);
    }
    if (!serverMQTT.check_connect())
    {
        WiFi_STATE = MODE_FAILD_CONNECT_MQTT;
        delay(1000);
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
// ======================================================
// CONFIG_STA - RECONNECT WIFI (STA ONLY)
// ======================================================
template <class Transport>
inline void PnP<Transport>::RECONNECT_WIFI()
{
    LOG_WIFI("WIFI", "RECONNECTING WIFI...");

    // Đồng bộ SSID từ bộ nhớ Flash NVS nếu biến tạm bị rỗng
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

    // Nếu không có bất kỳ SSID nào được cấu hình -> Chuyển sang AP để người dùng cấu hình
    if (strlen(_sta_ssid) == 0 && saved_ssid[0].length() == 0)
    {
        LOG_ERROR("WIFI", "NO SAVED WIFI FOUND! SWITCHING TO AP MODE...");
        WiFi_STATE = MODE_STARTUP_AP;
        return;
    }

    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_STA);

    // =========================
    // 1. THỬ WIFI HIỆN TẠI
    // =========================
    if (strlen(_sta_ssid) > 0)
    {
        WiFi.begin(_sta_ssid, _sta_pass);
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
            unsigned long t_ip = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t_ip < 3000)
            {
                delay(50);
            }
            LOG_WIFI("WIFI", "RECONNECT WIFI DONE (IP: %s)", WiFi.localIP().toString().c_str());
            WiFi_STATE = MODE_CONNECT_MQTT;
            return;
        }
    }

    // =========================
    // 2. FALLBACK WIFI TRONG BỘ NHỚ
    // =========================
    for (int i = 0; i < Saved_WiFi_MAX; i++)
    {
        if (saved_ssid[i].length() == 0 || saved_ssid[i] == _sta_ssid)
            continue;
        WiFi.disconnect(false);
        delay(100);
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
            unsigned long t_ip = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t_ip < 3000)
            {
                delay(50);
            }
            strncpy(_sta_ssid, saved_ssid[i].c_str(), sizeof(_sta_ssid) - 1);
            _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
            strncpy(_sta_pass, saved_pass[i].c_str(), sizeof(_sta_pass) - 1);
            _sta_pass[sizeof(_sta_pass) - 1] = '\0';
            LOG_WIFI("WIFI", "RECONNECT WIFI DONE (IP: %s)", WiFi.localIP().toString().c_str());
            WiFi_STATE = MODE_CONNECT_MQTT;
            return;
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
    // =========================
    // CHECK WIFI TRƯỚC
    // =========================
    if (WiFi.status() != WL_CONNECTED)
    {
        LOG_ERROR("MQTT", "NO WIFI -> SWITCH TO WIFI RECOVERY");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
        return;
    }
    // =========================
    // RESET MQTT STATE
    // =========================
    serverMQTT.disconnect();
    delay(100);
    serverMQTT.begin();
    LOG_MQTT("MQTT", "TRY RECONNECT MQTT...");
    unsigned long t_start = millis();
    unsigned long t_log = millis();
    while (!serverMQTT.check_connect() && millis() - t_start < 8000)
    {
        if (millis() - t_log > 1000)
        {
            LOG_MQTT("MQTT", "RECONNECTING... %ds", (millis() - t_start) / 1000);
            t_log = millis();
        }
        delay(50);
    }
    // =========================
    // SUCCESS
    // =========================
    if (serverMQTT.check_connect())
    {
        LOG_MQTT("MQTT", "MQTT RECONNECTED OK");
        SaveMQTT(mqttusername, mqttpass);
        WiFi_STATE = MODE_CONNECTED;
        return;
    }
    // =========================
    // FAIL
    // =========================
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
    if (lastMqttRetry == 0)
    {
        lastMqttRetry = millis();
    }

    // Nếu Wi-Fi bị mất trong lúc này -> Chuyển ngay sang chế độ phục hồi Wi-Fi
    if (WiFi.status() != WL_CONNECTED)
    {
        lastMqttRetry = 0;
        LOG_ERROR("MQTT", "WIFI LOST WHILE RECOVERING MQTT -> SWITCH TO WIFI RECOVERY");
        WiFi_STATE = MODE_LOST_CONNECT_WIFI;
        return;
    }

    // Nếu Wi-Fi vẫn còn nhưng MQTT rớt, tự động thử lại sau mỗi 5 giây
    if (millis() - lastMqttRetry >= 5000)
    {
        lastMqttRetry = 0;
        LOG_MQTT("MQTT", "RETRYING MQTT CONNECTION...");
        WiFi_STATE = MODE_LOST_CONNECT_MQTT;
    }
}

template <class Transport>
inline void PnP<Transport>::FAILD_WIFI()
{
    static unsigned long lastFailTime = 0;
    if (lastFailTime == 0)
    {
        lastFailTime = millis();
    }
    if (millis() - lastFailTime >= 5000)
    {
        lastFailTime = 0;
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
        ESP.restart(); });
    // Captive Portal redirection
    webServer.on("/generate_204", HTTP_GET, [this]()
                 { this->ConfigPage(); });
    webServer.on("/hotspot-detect.html", HTTP_GET, [this]()
                 { this->ConfigPage(); });
    webServer.on("/canonical.html", HTTP_GET, [this]()
                 { this->ConfigPage(); });
    webServer.onNotFound([this]()
                         { this->ConfigPage(); });

    webServer.begin();
    LOG_WIFI("AP", "WEB CONFIG READY AT http://%s", _ap_ip);
    WiFi_STATE = MODE_CONFIG;
}

//================ BUTTON ================//
template <class Transport>
inline void PnP<Transport>::handler_button()
{
#ifdef BUTTON_CONFIG

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
        {
            static unsigned long lastAPRetry = 0;
            if (lastAPRetry == 0)
            {
                lastAPRetry = millis();
            }
            // Định kỳ mỗi 15 giây, tự động kiểm tra xem WiFi đã lưu có hoạt động lại không
            if (millis() - lastAPRetry > 15000)
            {
                lastAPRetry = millis();
                loadWiFi();
                bool hasKnown = (strlen(_sta_ssid) > 0) || (saved_ssid[0].length() > 0);
                if (hasKnown)
                {
                    const char *targetSSID = (strlen(_sta_ssid) > 0) ? _sta_ssid : saved_ssid[0].c_str();
                    const char *targetPass = (strlen(_sta_ssid) > 0) ? _sta_pass : saved_pass[0].c_str();
                    LOG_WIFI("AP", "CHECKING IF KNOWN WIFI '%s' IS BACK ONLINE...", targetSSID);
                    WiFi.begin(targetSSID, targetPass);
                    unsigned long checkStart = millis();
                    while (WiFi.status() != WL_CONNECTED && millis() - checkStart < 8000)
                    {
                        dnsServer.processNextRequest();
                        webServer.handleClient();
                        delay(50);
                    }
                    if (WiFi.status() == WL_CONNECTED)
                    {
                        LOG_WIFI("AP", "RECONNECTED TO '%s'! CLOSING AP MODE...", targetSSID);
                        webServer.stop();
                        dnsServer.stop();
                        WiFi.softAPdisconnect(true);
                        WiFi.mode(WIFI_STA);
                        strncpy(_sta_ssid, targetSSID, sizeof(_sta_ssid) - 1);
                        _sta_ssid[sizeof(_sta_ssid) - 1] = '\0';
                        strncpy(_sta_pass, targetPass, sizeof(_sta_pass) - 1);
                        _sta_pass[sizeof(_sta_pass) - 1] = '\0';
                        WiFi_STATE = MODE_CONNECT_MQTT;
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