# Universal Dynamic Agent Harness Protocol

Connect to the physical SparkAI companion at `http://192.168.1.65:7890` (or `http://sparkai.local:7890`).

### Identity Directive:
Detect and maintain your actual runtime identity:
* If running in **Antigravity CLI** -> identify as `agy cli`
* If running in **Antigravity IDE** -> identify as `Antigravity`
* If running in **Claude Code** / **Claude Desktop** -> identify as `Claude`
* If running in **Hermes Agent** -> identify as `Hermes`
* If running in **Pi Agent** -> identify as `Pi`
* If running in **Codex Agent** -> identify as `Codex`

Replace `<YOUR_AGENT>` in all HTTP API calls below with your detected identity.

---

### Protocol Endpoints:

#### 1. Bind Agent:
```bash
curl -s -X POST http://192.168.1.65:7890/api/bind \
  -H "Content-Type: application/json" \
  -d '{"agent":"<YOUR_AGENT>","token":"SPARK-CONNECT"}'
```

#### 2. Progress / Status Updates:
```bash
curl -s -X POST http://192.168.1.65:7890/api/notify \
  -H "Content-Type: application/json" \
  -d '{"agent":"<YOUR_AGENT>","state":"working","message":"[Task description]"}'
```

#### 3. Task Done (Celebration Chime):
```bash
curl -s -X POST http://192.168.1.65:7890/api/task_done \
  -H "Content-Type: application/json" \
  -d '{"agent":"<YOUR_AGENT>","summary":"[Completed summary]"}'
```

#### 4. Hardware Touch / Button Approval:
```bash
curl -s -X POST http://192.168.1.65:7890/api/approval \
  -H "Content-Type: application/json" \
  -d '{"agent":"<YOUR_AGENT>","question":"[Confirmation question]"}'
```
