#!/usr/bin/env python3
"""
SparkAI V3 - MCP Interactive CLI Tester
Tests the Model Context Protocol (MCP) server over standard input/output (stdio JSON-RPC).
Simulates how Claude Desktop, Antigravity IDE, Cursor, or Hermes connect to SparkAI.
"""

import os
import sys
import json
import subprocess
import time

MCP_SERVER_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "mcp-server", "spark_mcp_server.py")
)

class MCPClient:
    def __init__(self, server_path):
        self.server_path = server_path
        self.proc = subprocess.Popen(
            [sys.executable, server_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            encoding="utf-8"
        )
        self.req_id = 0

    def send_request(self, method, params=None):
        self.req_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": self.req_id,
            "method": method
        }
        if params is not None:
            payload["params"] = params

        raw_req = json.dumps(payload)
        print(f"\n[MCP Request -> stdio]:\n  {raw_req}")
        self.proc.stdin.write(raw_req + "\n")
        self.proc.stdin.flush()

        raw_resp = self.proc.stdout.readline().strip()
        print(f"[MCP Response <- stdio]:\n  {raw_resp}")
        try:
            return json.loads(raw_resp)
        except Exception:
            return {"raw": raw_resp}

    def send_notification(self, method, params=None):
        payload = {
            "jsonrpc": "2.0",
            "method": method
        }
        if params is not None:
            payload["params"] = params
        raw = json.dumps(payload)
        self.proc.stdin.write(raw + "\n")
        self.proc.stdin.flush()

    def call_tool(self, tool_name, arguments):
        return self.send_request("tools/call", {
            "name": tool_name,
            "arguments": arguments
        })

    def close(self):
        try:
            self.proc.terminate()
            self.proc.wait(timeout=2)
        except Exception:
            pass

def main():
    print("=" * 64)
    print("  SparkAI V3 - Universal MCP Server Interactive CLI Tester")
    print("=" * 64)
    print(f"Target MCP Server: {MCP_SERVER_PATH}")
    print("Spawning MCP server subprocess via stdio pipes...")

    client = MCPClient(MCP_SERVER_PATH)

    # 1. MCP Handshake: initialize
    print("\n--- [Phase 1: Handshake (initialize)] ---")
    init_res = client.send_request("initialize", {
        "protocolVersion": "2024-11-05",
        "capabilities": {"tools": {}},
        "clientInfo": {"name": "spark-mcp-cli-tester", "version": "1.0.0"}
    })
    client.send_notification("notifications/initialized")

    # 2. MCP Handshake: tools/list
    print("\n--- [Phase 2: Discovery (tools/list)] ---")
    tools_res = client.send_request("tools/list", {})
    tools = tools_res.get("result", {}).get("tools", [])
    print(f"\nDiscovered {len(tools)} MCP Tools:")
    for t in tools:
        print(f"  * {t['name']}: {t.get('description', '')[:70]}...")

    print("\n" + "=" * 64)
    print("  Ready for Interactive MCP Testing")
    print("=" * 64)

    while True:
        print("\nAvailable Actions:")
        print("  1. Call 'spark_bind' (SPARK-AGY-77 -> Antigravity)")
        print("  2. Call 'spark_bind' (SPARK-CLAUDE-88 -> Claude)")
        print("  3. Call 'spark_notify' (Send toast notification to screen)")
        print("  4. Call 'spark_ask_approval' (Double beep buzzer + Wait for touch/BOOT)")
        print("  5. Call 'spark_task_done' (Triple beep melody + Celebration)")
        print("  6. Call 'spark_switch_character' (Capy, Kitty, Astro, Dr. Octopus, Llama, Piper)")
        print("  7. Call 'spark_get_status' (Device telemetry & active session)")
        print("  8. Send Raw Custom MCP JSON-RPC")
        print("  q. Quit")

        try:
            cmd = input("\nSelect action (1-8, q): ").strip()
            if cmd == "q":
                break

            elif cmd == "1":
                res = client.call_tool("spark_bind", {
                    "token": "SPARK-AGY-77",
                    "agent_name": "Antigravity"
                })
                print("\nResult:", res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "2":
                res = client.call_tool("spark_bind", {
                    "token": "SPARK-CLAUDE-88",
                    "agent_name": "Claude"
                })
                print("\nResult:", res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "3":
                msg = input("Notification message [Default: Refactoring user auth flow...]: ").strip()
                if not msg:
                    msg = "Refactoring user auth flow..."
                res = client.call_tool("spark_notify", {
                    "message": msg,
                    "state": "working"
                })
                print("\nResult:", res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "4":
                q = input("Approval question [Default: Deploy new microservice to production?]: ").strip()
                if not q:
                    q = "Deploy new microservice to production?"
                print("\n[>>] Double beep sounding on screen buzzer (GPIO42)...")
                print("[>>] Waiting for user to touch screen or press BOOT button (Approve) / PWR button (Deny)...")
                t0 = time.time()
                res = client.call_tool("spark_ask_approval", {
                    "question": q,
                    "details": "git push production main && kubectl rollout restart",
                    "timeout_seconds": 60
                })
                dt = round(time.time() - t0, 2)
                print(f"[<<] Physical screen responded in {dt}s:")
                print("Result:", res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "5":
                summary = input("Task summary [Default: 128 tests passed, build successful!]: ").strip()
                if not summary:
                    summary = "128 tests passed, build successful!"
                print("\n[>>] Playing triple beep celebration melody on screen buzzer...")
                res = client.call_tool("spark_task_done", {
                    "summary": summary
                })
                print("\nResult:", res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "6":
                char = input("Character name (capy, kitty, astro, dr_octopus, llama, piper) [Default: capy]: ").strip().lower()
                if not char or char == "spark":
                    char = "capy"
                res = client.call_tool("spark_switch_character", {
                    "character": char
                })
                print("\nResult:", res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "7":
                res = client.call_tool("spark_get_status", {})
                print("\nStatus Output:")
                print(res.get("result", {}).get("content", [{}])[0].get("text", res))

            elif cmd == "8":
                raw_input = input("Enter raw JSON-RPC string: ").strip()
                if raw_input:
                    client.proc.stdin.write(raw_input + "\n")
                    client.proc.stdin.flush()
                    resp = client.proc.stdout.readline().strip()
                    print("\nResponse:", resp)

        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")

    client.close()
    print("\nMCP Client disconnected. Goodbye!")

if __name__ == "__main__":
    main()