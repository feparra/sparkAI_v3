#!/usr/bin/env python3
"""
SparkAI V3 - Universal Model Context Protocol (MCP) Server
Allows any AI Agent (Claude Desktop, Antigravity IDE, Hermes, Cursor, Codex, OpenClaw)
to communicate bidirectionally with the SparkAI Physical Screen Companion.

Supports natural language binding:
  "Connect to SparkAI with token SPARK-AGY-77"
  "Connect to SparkAI with token SPARK-CLAUDE-88"
"""

import sys
import json
import urllib.request
import urllib.error

# Force UTF-8 on Windows
if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

import os
GATEWAY_PORT = int(os.environ.get("SPARK_PORT", 7890))
SPARK_HOST = os.environ.get("SPARK_HOST", f"http://localhost:{GATEWAY_PORT}").rstrip("/")

def send_gateway_request(endpoint, data=None, method=None, timeout=120):
    url = f"{SPARK_HOST}{endpoint}"
    req_data = json.dumps(data).encode('utf-8') if data is not None else None
    headers = {"Content-Type": "application/json"} if data is not None else {}
    req = urllib.request.Request(url, data=req_data, headers=headers)
    if method:
        req.method = method
    elif data is None:
        req.method = "GET"
    else:
        req.method = "POST"
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read().decode('utf-8'))
    except Exception as e:
        return {"ok": False, "error": f"Gateway error: {e}"}

TOOLS_LIST = [
    {
        "name": "spark_bind",
        "description": "Authenticates and binds this AI agent to the SparkAI physical screen companion using your dedicated access token. Call this when the user says 'Connect to SparkAI with token <TOKEN>'.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "token": {
                    "type": "string",
                    "description": "Your access token (e.g. SPARK-AGY-77, SPARK-CLAUDE-88, SPARK-HERMES-99, SPARK-CODEX-11)"
                },
                "agent_name": {
                    "type": "string",
                    "description": "Name of your agent (e.g. Antigravity, Claude, Hermes, Codex)"
                }
            },
            "required": ["token"]
        }
    },
    {
        "name": "spark_notify",
        "description": "Updates the SparkAI physical screen with a status update, toast message, or state change.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "message": {
                    "type": "string",
                    "description": "Short explanation of what you are doing (shown on screen)"
                },
                "state": {
                    "type": "string",
                    "enum": ["calm", "working", "waiting", "done", "error"],
                    "default": "working",
                    "description": "Emotional state of companion (e.g. working=typing animation, calm=resting)"
                },
                "agent": {
                    "type": "string",
                    "description": "Agent name to display on the screen badge"
                }
            },
            "required": ["message"]
        }
    },
    {
        "name": "spark_ask_approval",
        "description": "Requests human authorization before executing critical tasks (running bash commands, editing key files, deploying). Rings a DOUBLE BEEP on the physical screen buzzer, displays an approval card, and pauses until the user touches the screen or presses the physical BOOT button.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "question": {
                    "type": "string",
                    "description": "Clear question for the user (e.g. 'Allow running tests in background?')"
                },
                "details": {
                    "type": "string",
                    "description": "Optional snippet or command to be executed"
                },
                "options": {
                    "type": "array",
                    "items": {"type": "string"},
                    "default": ["Approve", "Deny"],
                    "description": "Options presented on screen"
                },
                "timeout_seconds": {
                    "type": "integer",
                    "default": 60,
                    "description": "Seconds to wait before timeout"
                }
            },
            "required": ["question"]
        }
    },
    {
        "name": "spark_task_done",
        "description": "Notifies the user that your task has successfully completed. Rings a TRIPLE BEEP melody on the physical screen buzzer and triggers a celebratory animation.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "summary": {
                    "type": "string",
                    "description": "Brief summary of what was completed"
                },
                "agent": {
                    "type": "string",
                    "description": "Agent name"
                }
            },
            "required": ["summary"]
        }
    },
    {
        "name": "spark_switch_character",
        "description": "Changes the animated companion character displayed on the Waveshare screen.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "character": {
                    "type": "string",
                    "enum": ["capy", "kitty", "astro", "dr_octopus", "llama", "piper"],
                    "description": "Companion character name"
                }
            },
            "required": ["character"]
        }
    },
    {
        "name": "spark_get_status",
        "description": "Returns current state of SparkAI physical screen, telemetry (battery, Wi-Fi), and active agent.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    }
]

def handle_tool_call(tool_name, arguments):
    if tool_name == "spark_bind":
        token = arguments.get("token", "")
        agent = arguments.get("agent_name", "")
        res = send_gateway_request("/api/bind", {"token": token, "agent": agent})
        if res.get("ok"):
            return f"Connected to SparkAI screen successfully! Agent: {res.get('agent')}. Active Character: {res.get('character')}. Notifications will now route directly to the physical screen."
        return f"Binding failed: {res.get('error', 'Unknown token')}"

    elif tool_name == "spark_notify":
        message = arguments.get("message", "")
        state = arguments.get("state", "working")
        agent = arguments.get("agent", "")
        res = send_gateway_request("/api/notify", {"message": message, "state": state, "agent": agent})
        return f"Screen updated ({state}): {message}"

    elif tool_name == "spark_ask_approval":
        question = arguments.get("question", "")
        details = arguments.get("details", "")
        options = arguments.get("options", ["Approve", "Deny"])
        timeout = arguments.get("timeout_seconds", 60)
        res = send_gateway_request("/api/approval", {
            "question": question,
            "details": details,
            "options": options,
            "timeout": timeout
        }, timeout=timeout + 5)
        
        choice = res.get("choice", "Timeout")
        source = res.get("source", "unknown")
        return f"Physical Screen Response: {choice} (Action received via {source})"

    elif tool_name == "spark_task_done":
        summary = arguments.get("summary", "Task Completed")
        agent = arguments.get("agent", "")
        res = send_gateway_request("/api/task_done", {"summary": summary, "agent": agent})
        return f"Task completion celebration triggered on screen with triple beep: {summary}"

    elif tool_name == "spark_switch_character":
        char = arguments.get("character", "capy").lower()
        if char == "spark":
            char = "capy"
        res = send_gateway_request("/api/character", {"character": char})
        return f"Active character switched to: {char}"

    elif tool_name == "spark_get_status":
        res = send_gateway_request("/api/status")
        return json.dumps(res, indent=2)

    return f"Unknown tool: {tool_name}"

def main():
    while True:
        try:
            line = sys.stdin.readline()
            if not line:
                break
            msg = json.loads(line)
            req_id = msg.get("id")
            method = msg.get("method")

            if method == "initialize":
                response = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "protocolVersion": "2024-11-05",
                        "capabilities": {"tools": {}},
                        "serverInfo": {"name": "sparkai-mcp-server", "version": "3.0.0"}
                    }
                }
            elif method == "tools/list":
                response = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {"tools": TOOLS_LIST}
                }
            elif method == "tools/call":
                params = msg.get("params", {})
                name = params.get("name")
                args = params.get("arguments", {})
                result_text = handle_tool_call(name, args)
                response = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "content": [{"type": "text", "text": result_text}]
                    }
                }
            elif method == "notifications/initialized":
                continue
            else:
                response = {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "error": {"code": -32601, "message": f"Method {method} not found"}
                }

            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()
        except Exception as e:
            err_resp = {
                "jsonrpc": "2.0",
                "id": None,
                "error": {"code": -32603, "message": str(e)}
            }
            sys.stdout.write(json.dumps(err_resp) + "\n")
            sys.stdout.flush()

if __name__ == '__main__':
    main()
