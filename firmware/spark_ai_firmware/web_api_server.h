#ifndef WEB_API_SERVER_H
#define WEB_API_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

class SparkApiServer {
public:
    typedef void (*CommandCallback)(const String& jsonCommand);

    SparkApiServer(uint16_t port = 7890) : server(port), cmdCb(nullptr) {}

    void begin(CommandCallback onCmd) {
        cmdCb = onCmd;
        setupRoutes();
        server.begin();
    }

    void handle() {
        server.handleClient();
    }

    void setStatusData(const String& agent, const String& character, const String& state, const String& message) {
        currentAgent = agent;
        currentCharacter = character;
        currentState = state;
        currentMessage = message;
    }

    void resolveApproval(const String& choice, const String& source) {
        pendingApprovalResolved = true;
        approvalChoice = choice;
        approvalSource = source;
    }

private:
    WebServer server;
    CommandCallback cmdCb;

    String currentAgent = "None";
    String currentCharacter = "capy";
    String currentState = "calm";
    String currentMessage = "Ready";

    bool isWaitingApproval = false;
    bool pendingApprovalResolved = false;
    String approvalChoice = "Timeout";
    String approvalSource = "timeout";

    void setCors() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    }

    void setupRoutes() {
        server.on("/api/status", HTTP_OPTIONS, [this]() {
            setCors();
            server.send(204);
        });

        server.on("/api/status", HTTP_GET, [this]() {
            setCors();
            JsonDocument doc;
            doc["ok"] = true;
            JsonObject session = doc["session"].to<JsonObject>();
            session["activeAgent"] = currentAgent;
            session["character"] = currentCharacter;
            session["state"] = currentState;
            session["message"] = currentMessage;
            session["wifi_ip"] = WiFi.localIP().toString();
            session["rssi"] = WiFi.RSSI();
            session["free_heap"] = ESP.getFreeHeap();

            String output;
            serializeJson(doc, output);
            server.send(200, "application/json", output);
        });

        server.on("/api/bind", HTTP_OPTIONS, [this]() { setCors(); server.send(204); });
        server.on("/api/bind", HTTP_POST, [this]() {
            setCors();
            if (!server.hasArg("plain")) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing body\"}");
                return;
            }
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String token = req["token"] | "";
            String agent = req["agent"] | "Agent";

            currentAgent = agent;
            JsonDocument cmd;
            cmd["event"] = "agent_connected";
            cmd["agent"] = agent;
            cmd["token"] = token;
            cmd["message"] = agent + " is online!";
            String cmdStr;
            serializeJson(cmd, cmdStr);
            if (cmdCb) cmdCb(cmdStr);

            server.send(200, "application/json", "{\"ok\":true,\"agent\":\"" + agent + "\"}");
        });

        server.on("/api/notify", HTTP_OPTIONS, [this]() { setCors(); server.send(204); });
        server.on("/api/notify", HTTP_POST, [this]() {
            setCors();
            if (!server.hasArg("plain")) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing body\"}");
                return;
            }
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String agent = req["agent"] | currentAgent.c_str();
            String state = req["state"] | "working";
            String msg = req["message"] | "";

            currentState = state;
            currentMessage = msg;

            JsonDocument cmd;
            cmd["event"] = "notification";
            cmd["agent"] = agent;
            cmd["state"] = state;
            cmd["message"] = msg;
            String cmdStr;
            serializeJson(cmd, cmdStr);
            if (cmdCb) cmdCb(cmdStr);

            server.send(200, "application/json", "{\"ok\":true,\"status\":\"dispatched\"}");
        });

        server.on("/api/character", HTTP_OPTIONS, [this]() { setCors(); server.send(204); });
        server.on("/api/character", HTTP_POST, [this]() {
            setCors();
            if (!server.hasArg("plain")) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing body\"}");
                return;
            }
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String character = req["character"] | "capy";
            character.toLowerCase();
            if (character == "spark") character = "capy";

            currentCharacter = character;

            JsonDocument cmd;
            cmd["event"] = "character_switch";
            cmd["character"] = character;
            String cmdStr;
            serializeJson(cmd, cmdStr);
            if (cmdCb) cmdCb(cmdStr);

            server.send(200, "application/json", "{\"ok\":true,\"character\":\"" + character + "\"}");
        });

        server.on("/api/task_done", HTTP_OPTIONS, [this]() { setCors(); server.send(204); });
        server.on("/api/task_done", HTTP_POST, [this]() {
            setCors();
            if (!server.hasArg("plain")) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing body\"}");
                return;
            }
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String summary = req["summary"] | "Task Completed!";
            String agent = req["agent"] | currentAgent.c_str();

            currentState = "done";
            currentMessage = summary;

            JsonDocument cmd;
            cmd["event"] = "task_complete";
            cmd["agent"] = agent;
            cmd["summary"] = summary;
            String cmdStr;
            serializeJson(cmd, cmdStr);
            if (cmdCb) cmdCb(cmdStr);

            server.send(200, "application/json", "{\"ok\":true,\"status\":\"celebration_triggered\"}");
        });

        server.on("/api/approval", HTTP_OPTIONS, [this]() { setCors(); server.send(204); });
        server.on("/api/approval", HTTP_POST, [this]() {
            setCors();
            if (!server.hasArg("plain")) {
                server.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing body\"}");
                return;
            }
            JsonDocument req;
            deserializeJson(req, server.arg("plain"));
            String question = req["question"] | "Approve task execution?";
            String id = req["id"] | "";
            if (id.isEmpty()) id = "appr_" + String(millis());
            int timeoutSec = req["timeout"] | 30;

            isWaitingApproval = true;
            pendingApprovalResolved = false;
            approvalChoice = "Timeout";
            approvalSource = "timeout";

            JsonDocument cmd;
            cmd["event"] = "approval_request";
            cmd["id"] = id;
            cmd["question"] = question;
            String cmdStr;
            serializeJson(cmd, cmdStr);
            if (cmdCb) cmdCb(cmdStr);

            // Block and wait for screen touch response or timeout
            uint32_t startWait = millis();
            while (!pendingApprovalResolved && (millis() - startWait < (uint32_t)timeoutSec * 1000)) {
                delay(50);
                yield();
            }

            isWaitingApproval = false;
            JsonDocument res;
            res["ok"] = (approvalChoice != "Timeout");
            res["choice"] = approvalChoice;
            res["source"] = approvalSource;
            String resStr;
            serializeJson(res, resStr);
            server.send(200, "application/json", resStr);
        });

        // Web Dashboard on GET /
        server.on("/", HTTP_GET, [this]() {
            String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                          "<title>SparkAI V3 On-Chip Hub</title>"
                          "<style>"
                          "body{background:#0F172A;color:#E2E8F0;font-family:sans-serif;margin:0;padding:24px;display:flex;justify-content:center;}"
                          ".card{background:#1E293B;border:1px solid #334155;border-radius:16px;max-width:440px;width:100%;padding:24px;box-shadow:0 10px 25px rgba(0,0,0,0.5);}"
                          "h1{color:#38BDF8;font-size:24px;margin-top:0;display:flex;align-items:center;gap:8px;}"
                          ".status-pill{display:inline-block;padding:6px 12px;border-radius:20px;font-size:12px;font-weight:700;margin-bottom:16px;}"
                          ".pill-online{background:#065F46;color:#34D399;}"
                          ".row{display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid #334155;font-size:14px;}"
                          ".val{font-weight:600;color:#F8FAFC;}"
                          ".btn-group{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px;}"
                          "button{padding:12px;background:#0284C7;color:#FFF;border:none;border-radius:8px;font-weight:600;cursor:pointer;}"
                          "button:hover{background:#0369A1;}"
                          "</style></head><body>"
                          "<div class='card'>"
                          "<h1>⚡ SparkAI V3 Screen</h1>"
                          "<div class='status-pill pill-online'>● STANDALONE ON-CHIP HUB ACTIVE</div>"
                          "<div class='row'><span>Active Agent:</span><span class='val'>" + currentAgent + "</span></div>"
                          "<div class='row'><span>Companion Character:</span><span class='val'>" + currentCharacter + "</span></div>"
                          "<div class='row'><span>Current State:</span><span class='val'>" + currentState + "</span></div>"
                          "<div class='row'><span>Message:</span><span class='val'>" + currentMessage + "</span></div>"
                          "<div class='row'><span>Wi-Fi IP Address:</span><span class='val'>" + WiFi.localIP().toString() + "</span></div>"
                          "<div class='row'><span>mDNS Hostname:</span><span class='val'>sparkai.local:7890</span></div>"
                          "<div class='btn-group'>"
                          "<button onclick='fetch(\"/api/notify\",{method:\"POST\",body:JSON.stringify({state:\"working\",message:\"Web Dashboard Ping!\"})})'>Send Ping</button>"
                          "<button onclick='fetch(\"/api/task_done\",{method:\"POST\",body:JSON.stringify({summary:\"Celebration from Web!\"})})'>Celebration Chime</button>"
                          "</div>"
                          "</div></body></html>";
            server.send(200, "text/html", html);
        });
    }
};

#endif // WEB_API_SERVER_H
