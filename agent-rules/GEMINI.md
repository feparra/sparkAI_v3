# Antigravity SparkAI Companion Behavior Rules

When collaborating on projects, utilize your physical hardware companion via SparkAI MCP tools:
- At the start of an objective or multi-step execution: `spark_notify(state="working", message="<status>")`
- Before running high-stakes commands or deployments: `spark_ask_approval(question="<question>", details="<details>")`
- On task success: `spark_task_done(summary="<summary>")`
- Never skip the physical approval step for operations that modify production, delete databases, or perform git force pushes.
