#ifndef WEB_API_SERVER_H
#define WEB_API_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <vector>

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
    std::vector<String> boundAgents;

    bool isWaitingApproval = false;
    bool pendingApprovalResolved = false;
    String approvalChoice = "Timeout";
    String approvalSource = "timeout";

    void setCors() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    }

    void setupRoutes() {
        // GET /api/status
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
            JsonArray arr = session["boundAgents"].to<JsonArray>();
            for (size_t i = 0; i < boundAgents.size(); i++) arr.add(boundAgents[i]);

            String res;
            serializeJson(doc, res);
            server.send(200, "application/json", res);
        });

        // POST /api/bind
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
            bool found = false;
            for (size_t i = 0; i < boundAgents.size(); i++) {
                if (boundAgents[i] == agent) { found = true; break; }
            }
            if (!found) boundAgents.push_back(agent);

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

        // POST /api/notify
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

        // POST /api/character
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

        // POST /api/task_done
        server.on("/api/task_done", HTTP_OPTIONS, [this]() { setCors(); server.send(204); });
        server.on("/api/task_done", HTTP_POST, [this]() {
            setCors();
            JsonDocument req;
            if (server.hasArg("plain")) {
                deserializeJson(req, server.arg("plain"));
            }
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

        // POST /api/approval
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
            cmd["agent"] = req["agent"] | currentAgent.c_str();
            cmd["question"] = question;
            cmd["timeout"] = timeoutSec;
            String cmdStr;
            serializeJson(cmd, cmdStr);
            if (cmdCb) cmdCb(cmdStr);

            uint32_t startWait = millis();
            while (!pendingApprovalResolved && (millis() - startWait < (uint32_t)(timeoutSec * 1000))) {
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
            String agentsList = "";
            if (boundAgents.empty()) {
                agentsList = (currentAgent != "None" && currentAgent.length() > 0) ? currentAgent : "None connected";
            } else {
                for (size_t i = 0; i < boundAgents.size(); i++) {
                    if (i > 0) agentsList += ", ";
                    agentsList += boundAgents[i];
                }
            }

            String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                          "<title>SparkAI V3 On-Chip Hub</title>"
                          "<style>"
                          "body{background:#0F172A;color:#E2E8F0;font-family:sans-serif;margin:0;padding:24px;display:flex;justify-content:center;}"
                          ".card{background:#1E293B;border:1px solid #334155;border-radius:16px;max-width:440px;width:100%;padding:24px;box-shadow:0 10px 25px rgba(0,0,0,0.5);box-sizing:border-box;}"
                          "h1{color:#38BDF8;font-size:24px;margin-top:0;display:flex;align-items:center;gap:8px;}"
                          ".status-pill{display:inline-block;padding:6px 12px;border-radius:20px;font-size:12px;font-weight:700;margin-bottom:16px;}"
                          ".pill-online{background:#065F46;color:#34D399;}"
                          ".row{display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid #334155;font-size:14px;gap:12px;}"
                          ".val{font-weight:600;color:#F8FAFC;text-align:right;}"
                          ".btn-group{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px;}"
                          "button{padding:12px;background:#0284C7;color:#FFF;border:none;border-radius:8px;font-weight:600;cursor:pointer;transition:all 0.2s;}"
                          "button:hover{background:#0369A1;}"
                          "#copyBtn{grid-column:span 2;background:linear-gradient(135deg,#0284C7,#2563EB);font-size:15px;padding:14px;font-weight:700;}"
                          "#copyBtn:hover{background:linear-gradient(135deg,#0369A1,#1D4ED8);}"
                          "</style></head><body>"
                          "<div class='card'>"
                          "<h1>⚡ SparkAI V3 Screen</h1>"
                          "<div class='status-pill pill-online'>● STANDALONE ON-CHIP HUB ACTIVE</div>"
                          "<div class='row'><span>Active Agent:</span><span class='val'>" + currentAgent + "</span></div>"
                          "<div class='row'><span>Connected Harnesses:</span><span class='val' style='color:#38BDF8;'>" + agentsList + "</span></div>"
                          "<div class='row'><span>Companion Character:</span><span class='val'>" + currentCharacter + "</span></div>"
                          "<div class='row'><span>Current State:</span><span class='val'>" + currentState + "</span></div>"
                          "<div class='row'><span>Message:</span><span class='val'>" + currentMessage + "</span></div>"
                          "<div class='row'><span>Wi-Fi IP Address:</span><span class='val'>" + WiFi.localIP().toString() + "</span></div>"
                          "<div class='row'><span>mDNS Hostname:</span><span class='val'>sparkai.local:7890</span></div>"
                          "<div class='row' style='align-items:center;'><span>Harness / Agent:</span>"
                          "<select id='agentSel' onchange='updatePromptPreview()' style='background:#0F172A;color:#38BDF8;border:1px solid #334155;border-radius:6px;padding:6px 10px;font-size:13px;font-weight:600;cursor:pointer;outline:none;'>"
                          "<option value='auto'>⚡ Auto-Detect (Dynamic)</option>"
                          "<option value='agy cli'>agy cli (Antigravity CLI)</option>"
                          "<option value='Antigravity'>Antigravity IDE</option>"
                          "<option value='Claude'>Claude Code / Desktop</option>"
                          "<option value='Hermes'>Hermes Agent</option>"
                          "<option value='Pi'>Pi Agent</option>"
                          "<option value='Codex'>Codex Agent</option>"
                          "</select></div>"
                          "<div class='btn-group'>"
                          "<button id='copyBtn' onclick='copyPrompt()'>📋 Connect Harness (Copy Prompt)</button>"
                          "<button onclick='fetch(\"/api/notify\",{method:\"POST\",body:JSON.stringify({state:\"working\",message:\"Web Dashboard Ping!\"})})'>Send Ping</button>"
                          "<button onclick='fetch(\"/api/task_done\",{method:\"POST\",body:JSON.stringify({summary:\"Celebration from Web!\"})})'>Celebration Chime</button>"
                          "</div>"
                          "<div id='promptBox' style='display:none;margin-top:16px;'>"
                          "<div style='font-size:12px;font-weight:700;color:#94A3B8;margin-bottom:6px;'>HARNESS PROMPT (CLICK TO SELECT ALL):</div>"
                          "<textarea id='promptArea' readonly onclick='this.select()' style='width:100%;height:150px;background:#0A0E1A;border:1px solid #334155;border-radius:8px;color:#38BDF8;font-family:monospace;font-size:12px;padding:8px;box-sizing:border-box;resize:none;'></textarea>"
                          "</div>"
                          "</div>"
                          "<script>"
                          "function getPrompt() {"
                          "  var h = window.location.host;"
                          "  var sel = document.getElementById('agentSel');"
                          "  var val = (sel && sel.value) ? sel.value : 'auto';"
                          "  if (val === 'auto') {"
                          "    return 'Yes, connect to my physical SparkAI companion at http://' + h + ' and follow this protocol:\\n\\n'"
                          "         + 'IMPORTANT IDENTITY DIRECTIVE:\\n'"
                          "         + 'Identify yourself dynamically using your actual runtime environment / agent identity (e.g. if you are running in Antigravity CLI, identify as \\'agy cli\\'; if Antigravity IDE, \\'Antigravity\\'; if Claude Code, \\'Claude\\'; if Hermes, \\'Hermes\\'; if Pi, \\'Pi\\'; if Codex, \\'Codex\\'). DO NOT assume you are Claude unless you are actually Claude Code. Replace YOUR_AGENT below with your actual runtime name in all calls:\\n\\n'"
                          "         + '1. Bind now:\\ncurl -s -X POST http://' + h + '/api/bind -H \\'Content-Type: application/json\\' -d \\'{\"agent\":\"YOUR_AGENT\",\"token\":\"SPARK-CONNECT\"}\\'\\n\\n'"
                          "         + '2. When working / executing tasks:\\ncurl -s -X POST http://' + h + '/api/notify -H \\'Content-Type: application/json\\' -d \\'{\"agent\":\"YOUR_AGENT\",\"state\":\"working\",\"message\":\"Task description...\"}\\'\\n\\n'"
                          "         + '3. When task is done:\\ncurl -s -X POST http://' + h + '/api/task_done -H \\'Content-Type: application/json\\' -d \\'{\"agent\":\"YOUR_AGENT\",\"summary\":\"Summary of completed work...\"}\\'\\n\\n'"
                          "         + '4. When asking confirmation / approval:\\ncurl -s -X POST http://' + h + '/api/approval -H \\'Content-Type: application/json\\' -d \\'{\"agent\":\"YOUR_AGENT\",\"question\":\"Approval question...\"}\\';';"
                          "  } else {"
                          "    var b1 = JSON.stringify({agent: val, token: 'SPARK-CONNECT'});"
                          "    var b2 = JSON.stringify({agent: val, state: 'working', message: 'Task description...'});"
                          "    var b3 = JSON.stringify({agent: val, summary: 'Summary of completed work...'});"
                          "    var b4 = JSON.stringify({agent: val, question: 'Approval question...'});"
                          "    return 'Yes, connect to my physical SparkAI companion at http://' + h + '. You are operating as \\'' + val + '\\'. Follow this protocol:\\n\\n'"
                          "         + '1. Bind now:\\ncurl -s -X POST http://' + h + '/api/bind -H \\'Content-Type: application/json\\' -d \\'' + b1 + '\\'\\n\\n'"
                          "         + '2. When working:\\ncurl -s -X POST http://' + h + '/api/notify -H \\'Content-Type: application/json\\' -d \\'' + b2 + '\\'\\n\\n'"
                          "         + '3. When task is done:\\ncurl -s -X POST http://' + h + '/api/task_done -H \\'Content-Type: application/json\\' -d \\'' + b3 + '\\'\\n\\n'"
                          "         + '4. When asking confirmation:\\ncurl -s -X POST http://' + h + '/api/approval -H \\'Content-Type: application/json\\' -d \\'' + b4 + '\\';';"
                          "  }"
                          "}"
                          "function updatePromptPreview() {"
                          "  var a = document.getElementById('promptArea');"
                          "  var b = document.getElementById('promptBox');"
                          "  if (a && b && b.style.display !== 'none') {"
                          "    a.value = getPrompt();"
                          "  }"
                          "}"
                          "function copyPrompt() {"
                          "  var t = getPrompt();"
                          "  var a = document.getElementById('promptArea');"
                          "  var b = document.getElementById('promptBox');"
                          "  var btn = document.getElementById('copyBtn');"
                          "  a.value = t;"
                          "  b.style.display = 'block';"
                          "  a.focus();"
                          "  a.select();"
                          "  a.setSelectionRange(0, 99999);"
                          "  var ok = false;"
                          "  try { ok = document.execCommand('copy'); } catch(e){}"
                          "  if (!ok && navigator.clipboard && navigator.clipboard.writeText) {"
                          "    navigator.clipboard.writeText(t).then(function(){ onDone(); }).catch(function(){ showSel(); });"
                          "  } else if (ok) {"
                          "    onDone();"
                          "  } else {"
                          "    showSel();"
                          "  }"
                          "}"
                          "function onDone() {"
                          "  var btn = document.getElementById('copyBtn');"
                          "  var sel = document.getElementById('agentSel');"
                          "  var label = (sel && sel.value !== 'auto') ? sel.value : 'Dynamic';"
                          "  btn.innerText = '✓ ' + label + ' Prompt Copied!';"
                          "  btn.style.background = '#059669';"
                          "  setTimeout(function() {"
                          "    btn.innerText = '📋 Connect Harness (Copy Prompt)';"
                          "    btn.style.background = 'linear-gradient(135deg,#0284C7,#2563EB)';"
                          "  }, 3000);"
                          "}"
                          "function showSel() {"
                          "  var btn = document.getElementById('copyBtn');"
                          "  btn.innerText = 'Prompt selected below - Press Ctrl+C / Cmd+C';"
                          "}"
                          "</script>"
                          "</body></html>";
            server.send(200, "text/html", html);
        });
    }
};

#endif // WEB_API_SERVER_H