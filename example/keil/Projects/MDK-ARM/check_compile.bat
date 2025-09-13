@echo off
echo Checking for Keil compiler...

REM Try different common Keil installation paths
set KEIL_PATH=""
if exist "C:\Keil_v5\UV4\UV4.exe" set KEIL_PATH="C:\Keil_v5\UV4\UV4.exe"
if exist "C:\Keil\UV4\UV4.exe" set KEIL_PATH="C:\Keil\UV4\UV4.exe"
if exist "C:\Keil_MDK\UV4\UV4.exe" set KEIL_PATH="C:\Keil_MDK\UV4\UV4.exe"

if %KEIL_PATH%=="" (
    echo Keil compiler not found in common paths
    echo Please check if Keil is installed and add to PATH
    pause
    exit /b 1
)

echo Found Keil at: %KEIL_PATH%
echo Compiling OpenLoad project...

%KEIL_PATH% -b OpenLoad.uvprojx -o compile_log.txt

if exist compile_log.txt (
    echo Compilation complete. Check compile_log.txt for results
    type compile_log.txt
) else (
    echo No log file generated
)

pause