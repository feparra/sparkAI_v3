# ✨ SparkAI V3 - Physical AI Companion Screen & Multi-Agent Bridge

SparkAI V3 connects your physical **Waveshare ESP32-S3-Touch-LCD-1.69** display to your autonomous AI coding agents (**Antigravity**, **Claude Code**, **Hermes**, **Codex**, **OpenClaw**).

Featuring **Capy** (chill capybara developer) with expressive animated emotions, instant physical human-in-the-loop approvals, hardware buzzer audio feedback, and natural language chat binding.

---

## 🚀 Key Features

- 🦫 **Expressive Animated Characters**: Default character is **Capy** (Chill Capybara developer) with smooth state transitions:
  - `calm`: Resting & listening for tasks
  - `working`: Energetically typing/coding
  - `waiting`: Waiting for user approval
  - `done`: Celebrating task completion
  - `error`: Warning / alert state
  *(Also includes Kitty Cyberpunk Cat, Astro, Dr. Octopus, Llama, and Piper!)*
- 🗣️ **Natural Language Agent Binding**: Tell any agent in chat:
  `"Connect to SparkAI with token SPARK-AGY-77"`
- 👥 **Multi-Agent Multiplexing**: Connect multiple agents simultaneously (Claude, Hermes, Antigravity, Codex) with dedicated color-coded badges and priority routing.
- 🔔 **Hardware Buzzer Sound Cadence (GPIO 42)**:
  - **Double Beep**: When an agent asks for human approval or needs help.
  - **Triple Beep Melody**: When a task finishes successfully.
  - **Tactile Click**: Haptic audio click on touch or button press.
- ⚡ **Two-Way Approvals (Human-In-The-Loop)**:
  - Tapping **[Approve]** or **[Deny]** on the CST816T capacitive touch screen.
  - Pressing physical **BOOT** button (GPIO 0) to instantly Approve.
  - Pressing physical **PWR** button (GPIO 40) to instantly Deny.
- 🔌 **Dual Connection Modes**:
  - **Mode A (Plug & Play USB)**: Zero setup, streams high-speed JSON over USB CDC (COM3).
  - **Mode B (Wi-Fi WebSocket)**: Direct wireless WebSocket link (`ws://host:7890/ws/device`).

---

## 📁 Repository Structure

```
sparkAI_v3/
├── assets/                  # High-quality animated character assets (GIFs & icons)
│   └── characters/          # capy, kitty, astro, dr_octopus, llama, piper
├── firmware/                # Waveshare ESP32-S3 firmware
│   ├── include/             # pin_config.h, buzzer.h, touch_cst816.h
│   ├── src/main.cpp         # Display, animations, touch, buttons, and JSON protocol
│   ├── data/                # LittleFS filesystem data with Capy animations
│   └── platformio.ini       # Build configuration for ESP32-S3 (16MB Flash, Octal PSRAM)
├── gateway/                 # Persistent Hub & Multi-Agent Event Bus (Node.js & Python)
│   ├── server.js            # HTTP REST + WebSocket server on port 7890
│   ├── tokens.json          # Multi-agent token registry (Antigravity, Claude, Hermes, Codex)
│   └── serial_screen_bridge.py # Auto-detecting COM3 USB bridge
├── mcp-server/              # Universal Model Context Protocol (MCP) Server
│   └── spark_mcp_server.py  # Tools: spark_bind, spark_notify, spark_ask_approval, spark_task_done
├── scripts/                 # Launchers and interactive testing CLI
│   ├── start_all.bat        # 1-Click launcher for Gateway + Serial Bridge
│   ├── start_gateway.bat    # Gateway server launcher
│   ├── start_serial_bridge.bat # Serial screen bridge launcher
│   └── spark_test_harness.py # Interactive CLI test tool
└── docs/                    # Complete English documentation
    ├── ARCHITECTURE.md      # Bidirectional communication architecture
    ├── NATURAL_LANGUAGE_GUIDE.md # Natural language chat binding guide
    ├── MCP_SETUP.md         # MCP configuration for Claude, Antigravity, Hermes
    └── HARDWARE_SPECS.md    # Pinout, buzzer frequencies, and touch specs
```

---

## ⚡ Quick Start

### 1. Launch the SparkAI Gateway & Hardware Bridge
Double-click:
```cmd
C:\Users\FERNA\Documents\sparkAI_v3\scripts\start_all.bat
```
*(Or launch `start_gateway.bat` and `start_serial_bridge.bat` separately).*

### 2. Test Interactively
Open a terminal and run the test harness:
```bash
python C:\Users\FERNA\Documents\sparkAI_v3\scripts\spark_test_harness.py
```
- Select `1` or `2` to bind Antigravity or Claude.
- Select `5` to test an approval request: your screen will play a **Double Beep**, show the question card, and wait for your touch or `BOOT` button!
- Select `6` to trigger task complete: your screen will play the **Triple Beep** celebration melody!

### 3. Connect Your AI Agents (Claude, Antigravity, Hermes)
Register the MCP server in your IDE configuration (`docs/MCP_SETUP.md`):
```json
{
  "mcpServers": {
    "sparkai-companion": {
      "command": "python",
      "args": ["C:/Users/FERNA/Documents/sparkAI_v3/mcp-server/spark_mcp_server.py"]
    }
  }
}
```

In any chat session, simply prompt:
> **"Connect to SparkAI with token SPARK-AGY-77"** (for Antigravity)  
> **"Connect to SparkAI with token SPARK-CLAUDE-88"** (for Claude)  
> **"Connect to SparkAI with token SPARK-HERMES-99"** (for Hermes)  

Your agent will bind immediately and use your physical screen for every progress update, human approval, and task celebration!
