@echo off
echo Waking up the Cyber-Pet Agent...
start "" pythonw "%~dp0pc-agent\pc_agent.py"
echo DeskBuddy Agent is now running silently in the background!
timeout /t 3 >nul
exit
