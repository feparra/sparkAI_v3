@echo off
title SparkAI V3 Gateway Hub
echo Starting SparkAI V3 Gateway Server...
cd /d "%~dp0..\gateway"
set NODE_PATH=%USERPROFILE%\.spark_desktop_runtime\node_modules;%~dp0..\gateway\node_modules
node server.js
pause
