/**
 * SparkAI V3 Gateway & Multi-Agent Event Hub
 * Bidirectional Persistent Bridge between AI Harnesses and Waveshare ESP32-S3 Screen.
 */

const os = require('os');
const path = require('path');

const userRuntimeModules = path.join(os.homedir(), '.spark_desktop_runtime', 'node_modules');
if (!module.paths.includes(userRuntimeModules)) {
  module.paths.push(userRuntimeModules);
}
const localModules = path.join(__dirname, 'node_modules');
if (!module.paths.includes(localModules)) {
  module.paths.push(localModules);
}

const http = require('http');
const url = require('url');
const fs = require('fs');

let WebSocket;
try {
  WebSocket = require('ws');
} catch (e) {
  console.warn('[SparkAI Gateway] WebSocket module not loaded, using fallback.');
}

const PORT = process.env.SPARK_PORT || 7890;
const TOKENS_PATH = path.join(__dirname, 'tokens.json');

class SparkGateway {
  constructor(port = PORT) {
    this.port = port;
    this.deviceSockets = new Set();
    this.harnessSockets = new Set();
    this.pendingApprovals = new Map(); // id -> { resolve, timer, agent, question }
    this.boundAgents = new Map(); // token -> agent info
    this.activeCharacter = 'capy'; // User preferred default: Chill Capybara
    this.currentSession = {
      activeAgent: 'None',
      character: 'capy',
      state: 'calm',
      message: 'SparkAI Capy is ready and listening...',
      lastUpdate: Date.now()
    };

    this.tokens = this.loadTokens();
    this.server = http.createServer((req, res) => this.handleHttp(req, res));

    if (WebSocket) {
      this.wss = new WebSocket.Server({ server: this.server });
      this.setupWebSockets();
    }
  }

  loadTokens() {
    try {
      if (fs.existsSync(TOKENS_PATH)) {
        let raw = fs.readFileSync(TOKENS_PATH, 'utf8');
        raw = raw.replace(/^\uFEFF/, '');
        const parsed = JSON.parse(raw);
        this.tokens = parsed.tokens || {};
        return this.tokens;
      }
    } catch (err) {
      console.error('[SparkAI Gateway] Error reading tokens.json:', err.message);
    }
    return {};
  }

  setupWebSockets() {
    this.wss.on('connection', (ws, req) => {
      const parsed = url.parse(req.url, true);
      const pathname = parsed.pathname;

      if (pathname === '/ws/device') {
        console.log('[SparkAI Gateway] Physical Screen connected via WebSocket!');
        this.deviceSockets.add(ws);

        // Send welcome / current state sync
        ws.send(JSON.stringify({
          event: 'sync',
          character: this.activeCharacter,
          state: this.currentSession.state,
          agent: this.currentSession.activeAgent,
          message: this.currentSession.message
        }));

        ws.on('message', (msg) => this.handleDeviceMessage(msg));
        ws.on('close', () => {
          console.log('[SparkAI Gateway] Physical Screen disconnected.');
          this.deviceSockets.delete(ws);
        });
      } else {
        // Harness / Agent Client WebSocket
        this.harnessSockets.add(ws);
        ws.on('message', (msg) => this.handleHarnessMessage(ws, msg));
        ws.on('close', () => this.harnessSockets.delete(ws));
      }
    });
  }

  handleDeviceMessage(raw) {
    try {
      const data = JSON.parse(raw.toString());
      console.log('[SparkAI Gateway] Incoming message from Screen:', data);

      if (data.event === 'approval_response' && data.id) {
        this.resolveApproval(data.id, data.choice || 'Approve', data.source || 'touch');
      } else if (data.event === 'heartbeat') {
        // Telemetry update (battery, wifi, etc.)
        this.currentSession.deviceTelemetry = {
          battery_mv: data.battery_mv,
          rssi: data.rssi,
          lastSeen: Date.now()
        };
      }
    } catch (e) {
      console.error('[SparkAI Gateway] Failed to parse device message:', e.message);
    }
  }

  handleHarnessMessage(ws, raw) {
    try {
      const data = JSON.parse(raw.toString());
      if (data.action === 'bind') {
        const result = this.bindAgent(data.token, data.agent);
        ws.send(JSON.stringify({ event: 'bind_response', ...result }));
      }
    } catch (e) {
      console.error('[SparkAI Gateway] Failed to parse harness message:', e.message);
    }
  }

  broadcastToDevices(payload) {
    const jsonStr = JSON.stringify(payload);
    for (const ws of this.deviceSockets) {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(jsonStr);
      }
    }
    // Also log for serial bridge or CLI monitors
    console.log('[SparkAI Device Broadcast]', jsonStr);
  }

  bindAgent(token, agentName) {
    this.loadTokens(); // Dynamic reload
    const tokenInfo = this.tokens[token];
    if (!tokenInfo) {
      return { ok: false, error: 'Invalid or unknown token: ' + token };
    }

    const assignedName = agentName || tokenInfo.agent;
    this.boundAgents.set(token, {
      ...tokenInfo,
      agent: assignedName,
      boundAt: Date.now()
    });

    this.currentSession.activeAgent = assignedName;
    this.currentSession.state = 'calm';
    this.currentSession.message = `${assignedName} connected!`;

    // Notify physical screen with connection chime
    this.broadcastToDevices({
      event: 'agent_connected',
      agent: assignedName,
      color: tokenInfo.color,
      icon: tokenInfo.icon,
      character: this.activeCharacter,
      message: `${assignedName} is online!`,
      buzzer: 'double_beep'
    });

    console.log(`[SparkAI Gateway] Agent bound: ${assignedName} (Token: ${token})`);
    return {
      ok: true,
      agent: assignedName,
      color: tokenInfo.color,
      character: this.activeCharacter,
      message: `Successfully connected ${assignedName} to SparkAI Screen.`
    };
  }

  requestApproval({ agent, question, options = ['Approve', 'Deny'], details = '', timeoutSec = 60 }) {
    const id = `appr_${Date.now()}_${Math.random().toString(36).substr(2, 4)}`;

    return new Promise((resolve) => {
      const timer = setTimeout(() => {
        if (this.pendingApprovals.has(id)) {
          console.log(`[SparkAI Gateway] Approval ${id} timed out.`);
          this.pendingApprovals.delete(id);
          this.broadcastToDevices({
            event: 'approval_timeout',
            id,
            character: this.activeCharacter,
            state: 'calm'
          });
          resolve({ ok: false, error: 'Timeout waiting for physical screen response', choice: 'Timeout' });
        }
      }, timeoutSec * 1000);

      this.pendingApprovals.set(id, { resolve, timer, agent, question });

      this.currentSession.state = 'waiting';
      this.currentSession.activeAgent = agent;
      this.currentSession.message = question;

      // Broadcast to physical screen
      // triggers double beep on GPIO42 buzzer, switches to waiting.gif, draws buttons
      this.broadcastToDevices({
        event: 'approval_request',
        id,
        agent,
        question,
        details,
        options,
        character: this.activeCharacter,
        state: 'waiting',
        buzzer: 'double_beep',
        buttons: {
          boot: options[0] || 'Approve',
          pwr: options[1] || 'Deny'
        }
      });
    });
  }

  resolveApproval(id, choice, source = 'screen') {
    const pending = this.pendingApprovals.get(id);
    if (!pending) return false;

    clearTimeout(pending.timer);
    this.pendingApprovals.delete(id);

    console.log(`[SparkAI Gateway] Approval ${id} resolved: choice="${choice}" via ${source}`);

    // Update screen state
    this.currentSession.state = choice === 'Approve' ? 'working' : 'calm';
    this.broadcastToDevices({
      event: 'approval_resolved',
      id,
      choice,
      source,
      character: this.activeCharacter,
      state: this.currentSession.state,
      buzzer: 'click'
    });

    pending.resolve({
      ok: true,
      id,
      choice,
      source,
      timestamp: Date.now()
    });

    return true;
  }

  setTaskDone({ agent, summary }) {
    this.currentSession.state = 'done';
    this.currentSession.activeAgent = agent || this.currentSession.activeAgent;
    this.currentSession.message = summary || 'Task Completed!';

    this.broadcastToDevices({
      event: 'task_complete',
      agent: this.currentSession.activeAgent,
      summary: this.currentSession.message,
      character: this.activeCharacter,
      state: 'done',
      buzzer: 'triple_beep'
    });

    // Reset to calm after 5 seconds
    setTimeout(() => {
      if (this.currentSession.state === 'done') {
        this.currentSession.state = 'calm';
        this.currentSession.message = `${this.currentSession.activeAgent} is ready.`;
        this.broadcastToDevices({
          event: 'state_change',
          character: this.activeCharacter,
          state: 'calm',
          message: this.currentSession.message
        });
      }
    }, 5000);
  }

  handleHttp(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

    if (req.method === 'OPTIONS') {
      res.writeHead(204);
      res.end();
      return;
    }

    const parsed = url.parse(req.url, true);
    const pathname = parsed.pathname;

    // GET /api/status
    if (req.method === 'GET' && pathname === '/api/status') {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({
        ok: true,
        session: this.currentSession,
        devicesConnected: this.deviceSockets.size,
        boundAgents: Array.from(this.boundAgents.values()),
        activeCharacter: this.activeCharacter,
        pendingApprovalsCount: this.pendingApprovals.size
      }));
      return;
    }

    // GET /api/tokens
    if (req.method === 'GET' && pathname === '/api/tokens') {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ ok: true, tokens: this.tokens }));
      return;
    }

    if (req.method === 'POST') {
      let body = '';
      req.on('data', chunk => body += chunk);
      req.on('end', async () => {
        let json = {};
        try {
          if (body) json = JSON.parse(body);
        } catch (e) {
          res.writeHead(400, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ ok: false, error: 'Invalid JSON body' }));
          return;
        }

        // POST /api/bind
        if (pathname === '/api/bind') {
          const { token, agent } = json;
          const result = this.bindAgent(token, agent);
          res.writeHead(result.ok ? 200 : 401, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify(result));
          return;
        }

        // POST /api/character (Switch character)
        if (pathname === '/api/character') {
          let { character } = json;
          if (character) {
            character = character.toLowerCase();
            if (character === 'spark') character = 'capy'; // Retired Spark companion
            this.activeCharacter = character;
            this.currentSession.character = this.activeCharacter;
            this.broadcastToDevices({
              event: 'character_switch',
              character: this.activeCharacter,
              state: this.currentSession.state
            });
          }
          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ ok: true, character: this.activeCharacter }));
          return;
        }

        // POST /api/notify (State & Toast notification)
        if (pathname === '/api/notify') {
          const { agent = this.currentSession.activeAgent, state = 'working', message = '', title = '' } = json;
          this.currentSession.activeAgent = agent;
          this.currentSession.state = state;
          this.currentSession.message = message || title;

          this.broadcastToDevices({
            event: 'notification',
            agent,
            state,
            title,
            message,
            character: this.activeCharacter
          });

          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ ok: true, status: 'dispatched' }));
          return;
        }

        // POST /api/approval (Ask human for physical screen approval)
        if (pathname === '/api/approval') {
          const { agent = this.currentSession.activeAgent, question, details, options, timeout = 60 } = json;
          if (!question) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ ok: false, error: 'Missing question parameter' }));
            return;
          }

          const result = await this.requestApproval({
            agent,
            question,
            details,
            options,
            timeoutSec: timeout
          });

          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify(result));
          return;
        }

        // POST /api/resolve (Called by Serial Bridge or Screen HTTP to resolve pending approval)
        if (pathname === '/api/resolve') {
          const { id, choice, source } = json;
          const success = this.resolveApproval(id, choice, source);
          res.writeHead(success ? 200 : 404, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ ok: success }));
          return;
        }

        // POST /api/task_done
        if (pathname === '/api/task_done') {
          const { agent, summary } = json;
          this.setTaskDone({ agent, summary });
          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ ok: true, status: 'celebration_triggered' }));
          return;
        }

        res.writeHead(404, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ ok: false, error: 'Endpoint not found' }));
      });
      return;
    }

    res.writeHead(404, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ ok: false, error: 'Not Found' }));
  }

  start() {
    this.server.listen(this.port, () => {
      console.log('====================================================');
      console.log(`[SparkAI V3 Gateway] Hub running on port ${this.port}`);
      console.log(`[SparkAI V3 Gateway] Active Character: ${this.activeCharacter.toUpperCase()}`);
      console.log(`[SparkAI V3 Gateway] Physical Screen WS: ws://localhost:${this.port}/ws/device`);
      console.log(`[SparkAI V3 Gateway] Agent Harness WS:  ws://localhost:${this.port}/ws/harness`);
      console.log('====================================================');
    });
  }
}

if (require.main === module) {
  const gateway = new SparkGateway();
  gateway.start();
}

module.exports = SparkGateway;
