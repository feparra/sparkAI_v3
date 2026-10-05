# SparkAI V3 - MCP (Model Context Protocol) Setup & Verification Guide

SparkAI V3 provides an official MCP Server (`mcp-server/spark_mcp_server.py`) that implements JSON-RPC 2.0 over standard input/output (`stdio`). This allows any AI agent or harness (Antigravity IDE, Claude Desktop, Claude Code, Hermes, Cursor, Codex) to communicate bidirectionally with the physical companion screen.

---

## 1. How to Test the MCP Server via Interactive CLI

You can test the MCP server directly from your terminal using the interactive tester CLI:

```bash
# Option A: Run python script
python scripts/test_mcp_cli.py

# Option B: Run batch file (Windows)
scripts\test_mcp.bat
```

### What `test_mcp_cli.py` does:
1. **Spawns Subprocess**: Launches `spark_mcp_server.py` over stdio pipes exactly like Claude or Antigravity does.
2. **Handshake Verification**: Sends `initialize` and `tools/list` to discover available tools.
3. **Interactive Menu**:
   - `1`: Bind Agent (`SPARK-AGY-77` -> Antigravity)
   - `2`: Bind Agent (`SPARK-CLAUDE-88` -> Claude)
   - `3`: Send Toast Notification (`spark_notify`)
   - `4`: Test Approval with Double Beep & Screen Wait (`spark_ask_approval`)
   - `5`: Test Task Complete Melody (`spark_task_done`)
   - `6`: Switch Character (`spark_switch_character`)
   - `7`: Check Device Status (`spark_get_status`)
   - `8`: Send custom raw JSON-RPC string

---

## 2. Registering MCP in AI Harnesses

### A. Antigravity IDE
In your MCP configuration file (e.g. `%USERPROFILE%\.gemini\antigravity-ide\mcp_config.json` or `.agents\mcp_config.json`):

```json
{
  "mcpServers": {
    "sparkai": {
      "command": "python",
      "args": [
        "path/to/sparkAI_v3/mcp-server/spark_mcp_server.py"
      ]
    }
  }
}
```

### B. Claude Desktop
In `%APPDATA%\Claude\claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "sparkai": {
      "command": "python",
      "args": [
        "path/to/sparkAI_v3/mcp-server/spark_mcp_server.py"
      ]
    }
  }
}
```

### C. Hermes / OpenClaw / Cursor
Provide the python command pointing to `spark_mcp_server.py`. The server automatically binds to standard I/O.

---

## 3. Natural Language Control in Chat

Once registered, you can control the physical screen in normal chat:

| You say | Agent action | Physical Screen Response |
| :--- | :--- | :--- |
| *"Connect to SparkAI with token SPARK-AGY-77"* | Calls `spark_bind` | Welcome chime, agent badge illuminates |
| *"Deploy the new build"* | Calls `spark_ask_approval` | Double beep, waiting animation, prompts for TOUCH or BOOT button |
| *"Work in the background on unit tests"* | Calls `spark_notify` | Typing animation, toast message on screen |
| *"I'm done with the refactoring"* | Calls `spark_task_done` | Triple beep celebration melody, Capy celebration |
| *"Change character to Kitty / Astro"* | Calls `spark_switch_character` | Animated character switches live on screen |