@echo off
title SparkAI V3 Launcher
echo Launching SparkAI V3 Hub and Hardware Screen Bridge...
start "SparkAI Gateway" cmd /k "%~dp0start_gateway.bat"
timeout /t 2 /nobreak >nul
start "SparkAI Hardware Bridge" cmd /k "%~dp0start_serial_bridge.bat" %*
echo SparkAI V3 is running!
