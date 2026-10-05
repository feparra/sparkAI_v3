#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

class SparkWiFiManager {
public:
    SparkWiFiManager() : server(80), dnsServer(), isApMode(false), isConnected(false) {}

    typedef void (*StatusCallback)(const String& status, const String& details);
    typedef void (*QrCallback)(const String& qrPayload, const String& title, const String& instructions);

    void begin(StatusCallback onStatus = nullptr, QrCallback onQr = nullptr) {
        statusCb = onStatus;
        qrCb = onQr;
        prefs.begin("spark_wifi", false);

        storedSsid = prefs.getString("ssid", "");
        storedPass = prefs.getString("pass", "");

        if (storedSsid.length() > 0) {
            connectToSavedWifi();
        } else {
            startCaptivePortal();
        }
    }

    void handle() {
        if (isApMode) {
            dnsServer.processNextRequest();
            server.handleClient();
        }
    }

    bool isWifiConnected() const {
        return isConnected;
    }

    String getLocalIp() const {
        return isConnected ? WiFi.localIP().toString() : (isApMode ? WiFi.softAPIP().toString() : "0.0.0.0");
    }

    String getSsid() const {
        return storedSsid;
    }

    void resetWifi() {
        prefs.remove("ssid");
        prefs.remove("pass");
        WiFi.disconnect(true, true);
        startCaptivePortal();
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

    void notifyQr(const String& payload, const String& title, const String& instructions) {
        if (qrCb) qrCb(payload, title, instructions);
    }

    void connectToSavedWifi() {
        isApMode = false;
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(storedSsid.c_str(), storedPass.c_str());

        notifyStatus("Connecting Wi-Fi", storedSsid);

        uint32_t startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
            delay(250);
            yield();
        }

        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            String ipStr = WiFi.localIP().toString();
            MDNS.begin("sparkai");
            MDNS.addService("http", "tcp", 7890);
            notifyStatus("Wi-Fi Connected!", "IP: " + ipStr);
        } else {
            notifyStatus("Wi-Fi Failed", "Starting Setup AP...");
            startCaptivePortal();
        }
    }

    void startCaptivePortal() {
        isApMode = true;
        isConnected = false;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("SparkAI-Setup");

        IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", apIP);

        setupWebServerRoutes();
        server.begin();

        notifyStatus("Wi-Fi Setup Mode", "Join: SparkAI-Setup");
        notifyQr("WIFI:S:SparkAI-Setup;;", "WIFI SETUP", "1. Join SparkAI-Setup\n2. Open 192.168.4.1");
    }

    void setupWebServerRoutes() {
        server.on("/", HTTP_GET, [this]() {
            server.send(200, "text/html", renderPortalHtml());
        });

        server.on("/connect", HTTP_POST, [this]() {
            String ssid = server.arg("ssid");
            String pass = server.arg("pass");
            String custom = server.arg("custom_ssid");
            if (custom.length() > 0) ssid = custom;

            if (ssid.length() == 0) {
                server.send(400, "text/html", "<h3>SSID cannot be empty</h3><a href='/'>Back</a>");
                return;
            }

            prefs.putString("ssid", ssid);
            prefs.putString("pass", pass);
            storedSsid = ssid;
            storedPass = pass;

            String successPage = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
                                "body{background:#0F172A;color:#E2E8F0;font-family:sans-serif;text-align:center;padding:40px;}"
                                "h2{color:#38BDF8;}.box{background:#1E293B;padding:24px;border-radius:12px;margin:20px auto;max-width:340px;border:1px solid #334155;}"
                                "</style></head><body><div class='box'><h2>Credentials Saved!</h2><p>SparkAI is connecting to <b>" + ssid + "</b>...</p><p>Check the screen for IP address!</p></div></body></html>";

            server.send(200, "text/html", successPage);
            delay(1000);
            ESP.restart();
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
                      ".badge{display:inline-block;background:#1E293B;color:#38BDF8;padding:4px 10px;border-radius:20px;font-size:12px;margin-bottom:14px;font-weight:600;}"
                      "</style></head><body>"
                      "<div class='card'>"
                      "<div style='text-align:center'><span class='badge'>SPARKAI HARDWARE COMPANION</span></div>"
                      "<h2>Connect to Wi-Fi</h2>"
                      "<p>Select your 2.4GHz Wi-Fi network so your Mac, Windows, and Server harnesses can connect directly to this screen.</p>"
                      "<form method='POST' action='/connect'>"
                      "<label for='ssid'>Network Name (SSID):</label>"
                      "<select name='ssid' id='ssid'>" + netOptions + "</select>"
                      "<label for='custom_ssid'>Or enter manually:</label>"
                      "<input type='text' name='custom_ssid' id='custom_ssid' placeholder='SSID if hidden'>"
                      "<label for='pass'>Wi-Fi Password:</label>"
                      "<input type='password' name='pass' id='pass' placeholder='••••••••'>"
                      "<button type='submit'>Save and Connect Screen</button>"
                      "</form>"
                      "</div>"
                      "<script>"
                      "document.getElementById('custom_ssid').addEventListener('input', function(e) {"
                      "  if (e.target.value.length > 0) { document.getElementById('ssid').value = ''; }"
                      "});"
                      "</script>"
                      "</body></html>";
        return html;
    }
};

#endif // WIFI_MANAGER_H
