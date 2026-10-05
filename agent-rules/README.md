# SparkAI V3 - Multi-Device & Harness Setup Guide

How to connect all your machines (Mac, Windows, Linux Server) to a single SparkAI Screen:

### 1. Connecting over Wi-Fi (Standalone Mode)
Set the environment variable on each machine:
```bash
# Mac / Linux
export SPARK_HOST="http://sparkai.local:7890"

# Windows PowerShell
$env:SPARK_HOST="http://sparkai.local:7890"
```

### 2. MCP Configuration (Claude Desktop / Cursor / Antigravity)
Add this to your `claude_desktop_config.json` or Cursor MCP settings:
```json
{
  "mcpServers": {
    "sparkai": {
      "command": "python",
      "args": ["/path/to/sparkAI_v3/mcp-server/spark_mcp_server.py"],
      "env": {
        "SPARK_HOST": "http://sparkai.local:7890"
      }
    }
  }
}
```

### 3. Pairing via On-Screen Token
1. Tap the top header of the Waveshare screen to generate a token (e.g. `SPARK-9281`).
2. Tell your agent in plain English:
   > "Connect to SparkAI with token SPARK-9281"
3. The screen will chime, display your agent badge, and bind permanently!
