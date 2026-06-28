@echo off
cd /d "%~dp0.."
where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo error: python not found — install Python 3.11+ from https://python.org
    pause
    exit /b 1
)
python tools\releng.py install
pause
