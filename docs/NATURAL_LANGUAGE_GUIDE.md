# SparkAI V3 - Natural Language Binding & Usage Guide

SparkAI V3 allows you to bind any AI agent harness (Antigravity IDE, Claude Code, Hermes, Codex, Cursor) to your physical companion screen using natural language directly inside the chat.

---

## 1. Connecting an Agent via Chat

Simply prompt your agent:

```text
Connect to SparkAI with token SPARK-AGY-77
```

or for Claude:

```text
Connect to SparkAI with token SPARK-CLAUDE-88
```

or for Hermes:

```text
Connect to SparkAI with token SPARK-HERMES-99
```

### What Happens Automatically:
1. The agent reads its MCP tool definition and executes `spark_bind(token="SPARK-...", agent_name="...")`.
2. The Gateway authenticates the token and updates the session registry.
3. The physical Waveshare screen:
   - Plays a pleasant rising connection chime on the onboard buzzer (GPIO42).
   - Displays the agent badge in its signature color (e.g. `[ ANTIGRAVITY ONLINE ]`).
   - Wakes up the active companion (Capy) to greet the agent.
4. The agent responds in chat:
   *"Connected to SparkAI screen successfully! Notifications and approvals will now route directly to your physical screen."*

---

## 2. Asking for Human Approval (Physical Human-In-The-Loop)

When an agent needs to execute a potentially risky action (e.g., executing shell scripts, migrating a database, committing to git), it calls `spark_ask_approval`.

### Physical Screen Interaction:
- **Audio**: Two crisp beeps (**Double Beep**) alert you across the room.
- **Visual**: Capy switches to `waiting.gif` and displays an authorization card with the question.
- **Physical Controls**:
  - **Press BOOT (GPIO0)**: Instant Approve.
  - **Press PWR (GPIO40)**: Instant Deny.
  - **Touch Screen**: Tap green [Approve] or red [Deny].
- Once pressed, a short tactile click sounds and the agent immediately resumes in your IDE!

---

## 3. Task Completion Celebration

When the agent finishes its plan or task, it automatically calls `spark_task_done(summary="...")`.
- **Audio**: A celebratory **Triple Beep** chime plays on the buzzer.
- **Visual**: Capy celebrates with `done.gif` and shows the summary of completed work.

---

## 4. Switching Characters on the Fly

You can ask the agent in chat anytime:

```text
Switch my Spark companion character to Kitty
```
or
```text
Switch my companion to Astro
```

Supported characters:
- `capy` (Default - Chill Capybara developer)
- `spark` (Retro Animated Bot)
- `kitty` (Cyberpunk Cat)
- `astro` (Cyber Astronaut)
- `dr_octopus` (Mad Cyber Scientist)
- `llama` (Neural Llama Copilot)
- `piper` (Cute Desktop Buddy)
