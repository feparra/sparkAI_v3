@echo off
title SparkAI V3 Serial Screen Bridge
set PORT=%1
if "%PORT%"=="" set PORT=COM3
echo Connecting to Waveshare Screen on %PORT%...
cd /d "%~dp0..\gateway"
python serial_screen_bridge.py %PORT%
pause
