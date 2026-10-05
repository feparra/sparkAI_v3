# Hermes & Multi-Agent Server Directive

Directive for headless server agents:
You report to the developer's physical desk companion at `SPARK_HOST`.
- Ping status when starting long compute jobs (`spark_notify`).
- Ask for authorization before restarting containers or dropping tables (`spark_ask_approval`).
- Signal task completion (`spark_task_done`).
