#!/usr/bin/env python3
"""
SparkAI V3 - Interactive Test Harness CLI
Tests two-way persistent communication between agents and the physical screen.
"""

import sys
import time
import json
import urllib.request

GATEWAY = "http://localhost:7890"

def post(endpoint, data):
    req = urllib.request.Request(
        f"{GATEWAY}{endpoint}",
        data=json.dumps(data).encode('utf-8'),
        headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=120) as resp:
        return json.loads(resp.read().decode('utf-8'))

def main():
    print("=" * 60)
    print("  SparkAI V3 Interactive Test Harness")
    print("=" * 60)
    print("1. Bind Agent (SPARK-AGY-77 -> Antigravity)")
    print("2. Bind Agent (SPARK-CLAUDE-88 -> Claude)")
    print("3. Bind Agent (SPARK-HERMES-99 -> Hermes)")
    print("4. Send Working State Notification")
    print("5. Test Approval (Double Beep + Screen Wait)")
    print("6. Test Task Complete (Triple Beep Done)")
    print("7. Switch Character (Capy, Kitty, Astro, Dr. Octopus, Llama, Piper)")
    print("8. Check Gateway Status")
    print("q. Exit")
    print("-" * 60)

    while True:
        try:
            choice = input("\nSelect test (1-8, q): ").strip()
            if choice == "q":
                break

            elif choice == "1":
                res = post("/api/bind", {"token": "SPARK-AGY-77", "agent": "Antigravity"})
                print("Result:", res)

            elif choice == "2":
                res = post("/api/bind", {"token": "SPARK-CLAUDE-88", "agent": "Claude"})
                print("Result:", res)

            elif choice == "3":
                res = post("/api/bind", {"token": "SPARK-HERMES-99", "agent": "Hermes"})
                print("Result:", res)

            elif choice == "4":
                msg = input("Enter status message: ") or "Running compilation tests..."
                res = post("/api/notify", {"state": "working", "message": msg})
                print("Result:", res)

            elif choice == "5":
                q = input("Question [Default: Allow running database migration?]: ") or "Allow running database migration?"
                print("\n[>>] Sending approval request to physical screen...")
                print("[>>] Double beep sounding on screen buzzer (GPIO42)...")
                print("[>>] Please TOUCH screen or press BOOT button (Approve) / PWR button (Deny)...")
                t0 = time.time()
                res = post("/api/approval", {"question": q, "options": ["Approve", "Deny"], "timeout": 60})
                dt = round(time.time() - t0, 2)
                print(f"[<<] Physical Screen Responded in {dt}s:")
                print(json.dumps(res, indent=2))

            elif choice == "6":
                summary = input("Task summary [Default: 42 unit tests passed]: ") or "42 unit tests passed"
                print("\n[>>] Triggering task complete + triple beep melody...")
                res = post("/api/task_done", {"summary": summary})
                print("Result:", res)

            elif choice == "7":
                char = input("Character (capy, kitty, astro, dr_octopus, llama, piper): ").strip() or "capy"
                if char.lower() == "spark": char = "capy"
                res = post("/api/character", {"character": char})
                print("Result:", res)

            elif choice == "8":
                req = urllib.request.Request(f"{GATEWAY}/api/status")
                with urllib.request.urlopen(req, timeout=5) as resp:
                    print(json.dumps(json.loads(resp.read().decode('utf-8')), indent=2))

        except KeyboardInterrupt:
            break
        except Exception as e:
            print("Error:", e)

if __name__ == '__main__':
    main()
