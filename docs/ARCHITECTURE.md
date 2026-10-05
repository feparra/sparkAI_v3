# SparkAI V3 - System Architecture & Bidirectional Protocol

SparkAI V3 transforms the physical Waveshare ESP32-S3-Touch-LCD-1.69 into an intelligent, animated hardware companion for autonomous AI coding agents (Antigravity, Claude Code, Hermes, Codex, OpenClaw).

---

## 1. High-Level Architecture

```
+--------------------------------------------------------------------------------+
|                             AI AGENT HARNESSES                                 |
|  [ Antigravity IDE ]     [ Claude Code ]     [ Hermes Agent ]     [ Codex ]    |
+--------------------------------------------------------------------------------+
                                       |
                                       | Model Context Protocol (stdio/JSON-RPC)
                                       v
+--------------------------------------------------------------------------------+
|                        SPARKAI MCP SERVER (`mcp-server/`)                      |
|   Tools: spark_bind, spark_notify, spark_ask_approval, spark_task_done         |
+--------------------------------------------------------------------------------+
                                       |
                                       | HTTP REST & WebSocket (Port 7890)
                                       v
+--------------------------------------------------------------------------------+
|                        SPARKAI GATEWAY HUB (`gateway/`)                        |
|   - Multi-Agent Token Registry (`tokens.json`)                                 |
|   - Multiplexing & Focus Manager                                              |
|   - Bidirectional Approval Promise Resolvers (Human-In-The-Loop)               |
|   - Audio & Animation Dispatcher                                               |
+--------------------------------------------------------------------------------+
                                       |
                   +-------------------+-------------------+
                   |                                       |
                   v (Wi-Fi WebSocket)                     v (USB Serial CDC @ 115200)
+--------------------------------------------------------------------------------+
|                WAVESHARE ESP32-S3-TOUCH-LCD-1.69 PHYSICAL DEVICE               |
|   - Animated Companion Engine (Capy Chill Developer, Kitty, etc.)       |
|   - Hardware Audio Synthesizer (GPIO42 Buzzer: Double & Triple Beep)           |
|   - CST816T Capacitive Touch Screen (Mirrored Calibration)                     |
|   - Physical Buttons: BOOT (GPIO0 = Approve) | PWR (GPIO40 = Deny)            |
+--------------------------------------------------------------------------------+
```

---

## 2. Solving Bidirectional Persistent Communication

### The Problem
Agents need to send state updates, notifications, and questions to the screen. Crucially, when an agent asks for human authorization (e.g., executing a command or deploying), the agent must pause and wait until the human taps the screen or presses a hardware button on the device.

### The Solution: The Persistent Event Bus & Async Resolvers

1. **Persistent Device Connection**:
   - The ESP32 screen connects to the Gateway via USB Serial CDC (COM3) or Wi-Fi WebSocket (`ws://host:7890/ws/device`).
   - The connection is persistent, kept alive with heartbeats.

2. **Async Approval Flow**:
   ```mermaid
   sequenceDiagram
       participant Agent as AI Agent (Claude/Antigravity)
       participant MCP as Spark MCP Server
       participant Hub as Spark Gateway Hub
       participant Screen as Physical Screen (ESP32)
       participant Human as User / Human

       Agent->>MCP: spark_ask_approval("Run bash tests?", options=["Approve", "Deny"])
       MCP->>Hub: POST /api/approval
       Hub->>Hub: Store pending Promise (id=appr_123, timeout=60s)
       Hub->>Screen: {"event":"approval_request", "id":"appr_123", "question":"Run bash tests?"}
       Screen->>Screen: Trigger Capy waiting.gif + GPIO42 Double Beep
       Screen->>Human: Display Question Card & Buttons
       alt Physical Button Pressed
           Human->>Screen: Press BOOT (GPIO0)
           Screen->>Hub: {"event":"approval_response", "id":"appr_123", "choice":"Approve", "source":"boot_button"}
       else Touch Screen Tapped
           Human->>Screen: Tap [Approve]
           Screen->>Hub: {"event":"approval_response", "id":"appr_123", "choice":"Approve", "source":"touch_screen"}
       end
       Hub->>Hub: Resolve Promise for appr_123
       Hub->>Screen: Play click tone + switch to working.gif
       Hub-->>MCP: { ok: true, choice: "Approve" }
       MCP-->>Agent: "Physical Screen Response: Approve"
       Agent->>Agent: Proceed with execution!
   ```

---

## 3. Multi-Agent Multiplexing

- **Multiple Concurrent Harnesses**: Unlimited agents can connect simultaneously.
- **Dedicated Tokens**: Each harness has a dedicated token:
  - `SPARK-AGY-77` -> Antigravity (Google Blue `#4285F4`)
  - `SPARK-CLAUDE-88` -> Claude (Amber `#D97706`)
  - `SPARK-HERMES-99` -> Hermes (Emerald `#10B981`)
  - `SPARK-CODEX-11` -> Codex (Purple `#8B5CF6`)
- **Visual Badge**: The screen top bar clearly indicates which agent currently owns the active session: `[ ANTIGRAVITY ]`, `[ CLAUDE ]`, etc.
- **Priority Routing**: Approval requests automatically preempt standard idle/working states to alert the user immediately.

---

## 4. Hardware Audio Signals (GPIO42 Buzzer)

| Sound Event | Cadence | Purpose |
|-------------|---------|---------|
| **Double Beep** | 950 Hz (100ms) -> 1350 Hz (160ms) | Question asked / Human approval required |
| **Triple Beep Done** | 784 Hz (90ms) -> 988 Hz (90ms) -> 1318 Hz (240ms) | Task finished / Celebratory completion |
| **Tactile Click** | 2400 Hz (15ms) | Touch registered or physical button pressed |
| **Connect Chime** | 880 Hz (80ms) -> 1175 Hz (120ms) | New agent harness bound with token |
| **Error Alert** | 350 Hz (350ms) | Task execution failed / Syntax error |
