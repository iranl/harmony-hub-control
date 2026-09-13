@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "HARMONY_KEY="
set /a HARMONY_KEY_COUNT=0

for %%F in ("%USERPROFILE%\.ssh\harmony_owner_*") do (
  if exist "%%~fF" (
    if /I not "%%~xF"==".pub" (
      set /a HARMONY_KEY_COUNT+=1
      if not defined HARMONY_KEY set "HARMONY_KEY=%%~fF"
    )
  )
)

if not defined HARMONY_KEY (
  echo Harmony SSH key not found.
  echo.
  echo Run the root tool first, or place the private key in:
  echo   %USERPROFILE%\.ssh\
  echo.
  echo The filename must start with harmony_owner_ and must not end in .pub.
  echo.
  pause
  exit /b 1
)

if !HARMONY_KEY_COUNT! GTR 1 (
  echo Found !HARMONY_KEY_COUNT! Harmony owner keys. Using:
  echo   !HARMONY_KEY!
  echo.
)

where pwsh >nul 2>nul
if %errorlevel%==0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_backup.ps1" -KeyPath "!HARMONY_KEY!" %*
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0restore_backup.ps1" -KeyPath "!HARMONY_KEY!" %*
)

echo.
if errorlevel 1 (
  echo Restore failed.
) else (
  echo Restore finished.
)
echo.
pause
