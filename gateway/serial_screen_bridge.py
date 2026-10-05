#!/usr/bin/env python3
"""
SparkAI V3 - USB Serial Hardware Screen Bridge
Connects the Waveshare ESP32-S3-Touch-LCD-1.69 over USB Serial (e.g. COM3)
to the SparkAI Gateway Hub WebSocket interface (ws://localhost:7890/ws/device).
"""

import sys
import time
import json
import threading
import serial
import serial.tools.list_ports
import urllib.request
import urllib.error

# WebSocket client using urllib or basic websocket
try:
    import websocket
except ImportError:
    websocket = None

DEFAULT_PORT = "COM3"
BAUD_RATE = 115200
GATEWAY_URL = "http://localhost:7890"

def find_esp32_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        desc = p.description.lower()
        if "ch340" in desc or "cp210" in desc or "usb jtag" in desc or "serial" in desc:
            if "com3" in p.device.lower():
                return p.device
    return DEFAULT_PORT

class SerialScreenBridge:
    def __init__(self, port=None):
        self.port = port or find_esp32_port()
        self.ser = None
        self.running = True

    def connect_serial(self):
        while self.running:
            try:
                print(f"[Serial Bridge] Attempting to open {self.port} at {BAUD_RATE} baud...")
                self.ser = serial.Serial(self.port, BAUD_RATE, timeout=0.1)
                print(f"[Serial Bridge] Successfully connected to Waveshare screen on {self.port}!")
                return True
            except Exception as e:
                print(f"[Serial Bridge] Waiting for {self.port} to be available: {e}")
                time.sleep(2)
        return False

    def forward_serial_to_gateway(self):
        """Reads incoming JSON from ESP32 screen and forwards to Spark Gateway"""
        buffer = ""
        while self.running:
            try:
                if self.ser and self.ser.is_open:
                    raw = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if raw and raw.startswith("{") and raw.endswith("}"):
                        print(f"[Screen -> Gateway] {raw}")
                        data = json.loads(raw)
                        # If it's an approval response, resolve it directly with the Gateway
                        if data.get("event") == "approval_response":
                            self.send_to_gateway("/api/resolve", data)
                        elif data.get("event") == "heartbeat":
                            # Device heartbeat
                            pass
                else:
                    time.sleep(0.5)
            except Exception as e:
                # Serial glitch or disconnect
                time.sleep(0.1)

    def send_to_gateway(self, endpoint, payload):
        try:
            req = urllib.request.Request(
                f"{GATEWAY_URL}{endpoint}",
                data=json.dumps(payload).encode('utf-8'),
                headers={"Content-Type": "application/json"}
            )
            with urllib.request.urlopen(req, timeout=5) as resp:
                pass
        except Exception as e:
            print(f"[Serial Bridge] Gateway HTTP error: {e}")

    def run(self):
        if not self.connect_serial():
            return

        # Start thread to read from serial
        t = threading.Thread(target=self.forward_serial_to_gateway, daemon=True)
        t.start()

        # Connect to Gateway WebSocket to receive events and send down to ESP32
        if websocket:
            self.run_websocket()
        else:
            print("[Serial Bridge] websocket-client not installed. Polling HTTP status fallback.")
            self.poll_gateway_status()

    def run_websocket(self):
        def on_message(ws, message):
            print(f"[Gateway -> Screen] {message}")
            if self.ser and self.ser.is_open:
                self.ser.write((message + "\n").encode('utf-8'))
                self.ser.flush()

        def on_error(ws, error):
            print(f"[Serial Bridge] WebSocket error: {error}")

        def on_close(ws, close_status_code, close_msg):
            print("[Serial Bridge] WebSocket connection to Gateway closed.")

        def on_open(ws):
            print("[Serial Bridge] Connected to Gateway WebSocket ws://localhost:7890/ws/device")

        while self.running:
            try:
                ws = websocket.WebSocketApp(
                    "ws://localhost:7890/ws/device",
                    on_open=on_open,
                    on_message=on_message,
                    on_error=on_error,
                    on_close=on_close
                )
                ws.run_forever()
            except Exception as e:
                print(f"[Serial Bridge] WebSocket reconnecting in 2s: {e}")
                time.sleep(2)

    def poll_gateway_status(self):
        last_state = ""
        while self.running:
            try:
                req = urllib.request.Request(f"{GATEWAY_URL}/api/status")
                with urllib.request.urlopen(req, timeout=2) as resp:
                    data = json.loads(resp.read().decode('utf-8'))
                    state_sig = f"{data.get('session', {}).get('state')}_{data.get('session', {}).get('message')}"
                    if state_sig != last_state:
                        last_state = state_sig
                        msg = json.dumps({
                            "event": "state_change",
                            "state": data.get('session', {}).get('state', 'calm'),
                            "character": data.get('activeCharacter', 'capy'),
                            "agent": data.get('session', {}).get('activeAgent', 'System'),
                            "message": data.get('session', {}).get('message', '')
                        }) + "\n"
                        if self.ser and self.ser.is_open:
                            self.ser.write(msg.encode('utf-8'))
                            self.ser.flush()
            except Exception:
                pass
            time.sleep(0.5)

if __name__ == '__main__':
    port = sys.argv[1] if len(sys.argv) > 1 else find_esp32_port()
    bridge = SerialScreenBridge(port)
    bridge.run()
