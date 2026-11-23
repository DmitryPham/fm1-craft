@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo FM-1 CRAFT: building the firmware in WSL (Ubuntu)...
wsl bash ./build_wsl.sh
echo.
if exist out\craft.fwsc (echo Firmware: %~dp0out\craft.fwsc) else (echo Build failed - send a screenshot of this window.)
pause
