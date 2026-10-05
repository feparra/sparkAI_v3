#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <lvgl.h>

class SparkWiFiManager {
public:
    SparkWiFiManager() : server(80), dnsServer(), isApMode(true), isConnected(false) {}

    typedef void (*StatusCallback)(const String& status, const String& details);
    typedef void (*QrCallback)(const String& qrPayload, const String& title, const String& instructions, const String& subText);

    void begin(StatusCallback onStatus = nullptr, QrCallback onQr = nullptr) {
        statusCb = onStatus;
        qrCb = onQr;

        // Dual AP + STA mode: AP is always ready on 192.168.4.1 as direct hub!
        WiFi.mode(WIFI_AP_STA);
        esp_wifi_set_ps(WIFI_PS_NONE);
        WiFi.setSleep(false);
        WiFi.setAutoReconnect(true);

        IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP("SparkAI-Setup");

        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", apIP);
        setupWebServerRoutes();
        server.begin();

        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
                Serial.printf("[WiFi Event] Disconnected, reason: %d\n", info.wifi_sta_disconnected.reason);
            } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
                Serial.println("[WiFi Event] Associated with AP!");
            } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
                Serial.printf("[WiFi Event] Got IP: %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
            }
        });

        prefs.begin("spark_wifi", false);
        storedSsid = prefs.getString("ssid", "");
        storedPass = prefs.getString("pass", "");
        prefs.end();

        Serial.printf("[WiFi] Stored SSID: '%s'\n", storedSsid.c_str());

        if (storedSsid.length() > 0) {
            connectToSavedWifi();
        } else {
            notifyStatus("Wi-Fi Setup Mode", "Join: SparkAI-Setup");
            notifyQr("WIFI:S:SparkAI-Setup;;", "[ WI-FI SETUP ]", "1. Join 'SparkAI-Setup'\n2. Open 192.168.4.1", "SparkAI-Setup");
        }
    }

    void handle() {
        dnsServer.processNextRequest();
        server.handleClient();

        if (storedSsid.length() > 0 && WiFi.status() != WL_CONNECTED) {
            static uint32_t lastRetry = 0;
            if (millis() - lastRetry > 8000) {
                lastRetry = millis();
                Serial.printf("[WiFi] Background reconnect to '%s'...\n", storedSsid.c_str());
                WiFi.begin(storedSsid.c_str(), storedPass.c_str());
            }
        }
    }

    bool isWifiConnected() const {
        return (WiFi.status() == WL_CONNECTED);
    }

    String getLocalIp() const {
        if (WiFi.status() == WL_CONNECTED) {
            String ip = WiFi.localIP().toString();
            if (ip != "0.0.0.0" && ip.length() > 0) return ip;
        }
        return "192.168.4.1";
    }

    String getSsid() const {
        return storedSsid;
    }

    void resetWifi() {
        prefs.begin("spark_wifi", false);
        prefs.remove("ssid");
        prefs.remove("pass");
        prefs.end();
        storedSsid = "";
        storedPass = "";
        WiFi.disconnect();
        notifyStatus("Wi-Fi Setup Mode", "Join: SparkAI-Setup");
        notifyQr("WIFI:S:SparkAI-Setup;;", "[ WI-FI SETUP ]", "1. Join 'SparkAI-Setup'\n2. Open 192.168.4.1", "SparkAI-Setup");
    }

    bool connectToSavedWifi() {
        Serial.printf("[WiFi] Connecting to '%s'...\n", storedSsid.c_str());
        notifyStatus("Connecting Wi-Fi", storedSsid);

        WiFi.begin(storedSsid.c_str(), storedPass.c_str());

        uint32_t startAttempt = millis();
        int lastSec = -1;
        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < 15000) {
            int elapsed = (millis() - startAttempt) / 1000;
            if (elapsed != lastSec) {
                lastSec = elapsed;
                notifyStatus("Connecting Wi-Fi", storedSsid + " (" + String(elapsed) + "s)");
            }
            delay(100);
            yield();
            lv_timer_handler();
        }

        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            String ipStr = WiFi.localIP().toString();
            Serial.printf("[WiFi] Connected! IP: %s\n", ipStr.c_str());
            MDNS.begin("sparkai");
            MDNS.addService("http", "tcp", 7890);
            notifyStatus("Wi-Fi Connected!", "IP: " + ipStr);
            return true;
        } else {
            Serial.printf("[WiFi] Timeout connecting to '%s'. AP Hub active at 192.168.4.1\n", storedSsid.c_str());
            notifyStatus("Wi-Fi: Retrying", "Direct Hub: 192.168.4.1");
            return false;
        }
    }

private:
    WebServer server;
    DNSServer dnsServer;
    Preferences prefs;
    String storedSsid;
    String storedPass;
    bool isApMode;
    bool isConnected;
    StatusCallback statusCb;
    QrCallback qrCb;

    void notifyStatus(const String& status, const String& details) {
        if (statusCb) statusCb(status, details);
    }

    void notifyQr(const String& payload, const String& title, const String& instructions, const String& subText) {
        if (qrCb) qrCb(payload, title, instructions, subText);
    }

    void setupWebServerRoutes() {
        server.on("/", HTTP_GET, [this]() {
            server.send(200, "text/html", renderPortalHtml());
        });

        // Live connection test API for the captive portal: phone never drops connection!
        server.on("/api/connect_test", HTTP_POST, [this]() {
            String ssid = server.arg("ssid");
            String pass = server.arg("pass");
            ssid.trim();
            pass.trim();

            if (ssid.length() == 0) {
                server.send(400, "application/json", "{\"success\":false,\"error\":\"SSID is required\"}");
                return;
            }

            Serial.printf("[Portal] Testing connection to '%s'...\n", ssid.c_str());
            WiFi.begin(ssid.c_str(), pass.c_str());

            uint32_t t0 = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 10000) {
                delay(200);
                yield();
            }

            if (WiFi.status() == WL_CONNECTED) {
                String ip = WiFi.localIP().toString();
                prefs.begin("spark_wifi", false);
                prefs.putString("ssid", ssid);
                prefs.putString("pass", pass);
                prefs.end();
                storedSsid = ssid;
                storedPass = pass;
                isConnected = true;
                notifyStatus("Wi-Fi Connected!", "IP: " + ip);
                server.send(200, "application/json", "{\"success\":true,\"ip\":\"" + ip + "\"}");
            } else {
                server.send(200, "application/json", "{\"success\":false,\"error\":\"Could not connect (Reason " + String(WiFi.status()) + "). Check password or select another network.\"}");
            }
        });

        server.on("/connect", HTTP_POST, [this]() {
            String ssid = server.arg("ssid");
            String pass = server.arg("pass");
            String custom = server.arg("custom_ssid");
            if (custom.length() > 0) ssid = custom;
            ssid.trim();
            pass.trim();

            if (ssid.length() == 0) {
                server.send(400, "text/html", "<h3>SSID cannot be empty</h3><a href='/'>Back</a>");
                return;
            }

            prefs.begin("spark_wifi", false);
            prefs.putString("ssid", ssid);
            prefs.putString("pass", pass);
            prefs.end();

            storedSsid = ssid;
            storedPass = pass;

            String successPage = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
                                "body{background:#0F172A;color:#E2E8F0;font-family:sans-serif;text-align:center;padding:40px;}"
                                "h2{color:#38BDF8;}.box{background:#1E293B;padding:24px;border-radius:12px;margin:20px auto;max-width:340px;border:1px solid #334155;}"
                                "</style></head><body><div class='box'><h2>Credentials Saved!</h2><p>Connecting to <b>" + ssid + "</b>...</p><p>Check the screen for IP address!</p></div></body></html>";

            server.send(200, "text/html", successPage);
            delay(1500);
            connectToSavedWifi();
        });

        // Captive portal detection redirects
        server.onNotFound([this]() {
            server.sendHeader("Location", "http://192.168.4.1/", true);
            server.send(302, "text/plain", "");
        });
    }

    String renderPortalHtml() {
        int n = WiFi.scanNetworks();
        String netOptions = "";
        for (int i = 0; i < n; ++i) {
            String itemSsid = WiFi.SSID(i);
            if (itemSsid.length() > 0) {
                netOptions += "<option value='" + itemSsid + "'>" + itemSsid + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
            }
        }

        String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                      "<title>SparkAI V3 Wi-Fi Setup</title>"
                      "<style>"
                      "body{background:#0B0F19;color:#F8FAFC;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;}"
                      ".card{background:#131B2E;border:1px solid #233554;border-radius:16px;max-width:380px;width:100%;padding:28px;box-sizing:border-box;box-shadow:0 10px 30px rgba(0,0,0,0.5);}"
                      "h2{margin-top:0;color:#38BDF8;font-size:22px;text-align:center;letter-spacing:0.5px;}"
                      "p{color:#94A3B8;font-size:14px;line-height:1.5;text-align:center;margin-bottom:24px;}"
                      "label{display:block;margin-top:14px;margin-bottom:6px;font-size:13px;color:#CBD5E1;font-weight:600;}"
                      "select,input{width:100%;padding:12px;box-sizing:border-box;background:#0A0E1A;border:1px solid #334155;border-radius:8px;color:#FFFFFF;font-size:15px;margin-bottom:12px;outline:none;}"
                      "select:focus,input:focus{border-color:#38BDF8;}"
                      "button{width:100%;padding:14px;background:#0284C7;background:linear-gradient(135deg,#0284C7,#0369A1);color:#FFFFFF;border:none;border-radius:8px;font-size:16px;font-weight:700;cursor:pointer;margin-top:16px;}"
                      "button:hover{background:#0369A1;}"
                      "#statusBox{display:none;padding:12px;border-radius:8px;margin-top:16px;font-size:14px;text-align:center;}"
                      ".badge{display:inline-block;background:#1E293B;color:#38BDF8;padding:4px 10px;border-radius:20px;font-size:12px;margin-bottom:14px;font-weight:600;}"
                      "</style></head><body>"
                      "<div class='card'>"
                      "<div style='text-align:center'><span class='badge'>SPARKAI HARDWARE COMPANION</span></div>"
                      "<h2>Connect to Wi-Fi</h2>"
                      "<p>Select your 2.4GHz Wi-Fi network (or mobile hotspot) so your Mac, Windows, and Server harnesses can connect directly to this screen.</p>"
                      "<form id='wifiForm' method='POST' action='/connect'>"
                      "<label for='ssid'>Network Name (SSID):</label>"
                      "<select name='ssid' id='ssid'>" + netOptions + "</select>"
                      "<label for='custom_ssid'>Or enter manually:</label>"
                      "<input type='text' name='custom_ssid' id='custom_ssid' placeholder='SSID if hidden'>"
                      "<label for='pass'>Wi-Fi Password:</label>"
                      "<input type='password' name='pass' id='pass' placeholder='••••••••'>"
                      "<button type='submit' id='subBtn'>Save and Connect Screen</button>"
                      "<div id='statusBox'></div>"
                      "</form>"
                      "</div>"
                      "<script>"
                      "document.getElementById('custom_ssid').addEventListener('input', function(e) {"
                      "  if (e.target.value.length > 0) { document.getElementById('ssid').value = ''; }"
                      "});"
                      "document.getElementById('wifiForm').addEventListener('submit', function(e) {"
                      "  var s = document.getElementById('subBtn');"
                      "  var box = document.getElementById('statusBox');"
                      "  s.disabled = true;"
                      "  s.innerText = 'Connecting... Please wait';"
                      "  box.style.display = 'block';"
                      "  box.style.background = '#1E293B';"
                      "  box.style.color = '#38BDF8';"
                      "  box.innerText = 'Testing connection with SparkAI screen...';"
                      "});"
                      "</script>"
                      "</body></html>";
        return html;
    }
};

#endif // WIFI_MANAGER_H
