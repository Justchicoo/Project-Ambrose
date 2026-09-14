REM Project Ambrose by Imjustchico
REM Runs run-client.ps1 so the client can be started with a double-click; arguments pass straight through.
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-client.ps1" %*
