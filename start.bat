@echo off
rem Satu klik (Windows): jalankan Node-RED, buka dashboard, lalu jalankan simulasi.
rem   start.bat             Wokwi (jika wokwi\wokwi-url.txt berisi link proyek), selain itu simulator lokal
rem   start.bat --lokal     selalu pakai simulator lokal (tanpa Wokwi)
rem   start.bat --tanpa-sim hanya Node-RED (data dari perangkat ESP32 nyata)
rem   set FLOW=flows-baru.json lalu start.bat   pakai flow baru kelompok
rem Tutup jendela "Node-RED" dan "Simulator" untuk menghentikan.
setlocal EnableDelayedExpansion
set "ROOT=%~dp0"
set "MODE=%~1"
if "%PORT%"=="" set PORT=1880
if "%FLOW%"=="" set FLOW=flows.json
cd /d "%ROOT%node-red"

where node >nul 2>nul
if errorlevel 1 (
  echo Node.js belum terpasang. Unduh di https://nodejs.org ^(versi 20 atau lebih baru^).
  pause
  exit /b 1
)

rem Token Telegram dll. (opsional) dari file .env
if exist "%ROOT%.env" (
  for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%ROOT%.env") do set "%%a=%%b"
)

if not exist node_modules (
  echo Instalasi pertama: memasang Node-RED dan dashboard...
  call npm install --no-audit --no-fund
)

start "Node-RED" cmd /k npx node-red --userDir . -p %PORT% %FLOW%

echo Menunggu Node-RED di port %PORT%...
:tunggu
node -e "fetch('http://localhost:%PORT%/ui/').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"
if errorlevel 1 (
  timeout /t 1 /nobreak >nul
  goto tunggu
)
start "" "http://localhost:%PORT%/ui/"
echo Dashboard : http://localhost:%PORT%/ui/
echo Editor    : http://localhost:%PORT%/

set "WOKWI_URL="
if exist "%ROOT%wokwi\wokwi-url.txt" (
  for /f "usebackq tokens=*" %%u in ("%ROOT%wokwi\wokwi-url.txt") do (
    set "baris=%%u"
    if "!baris:~0,18!"=="https://wokwi.com/" if "!WOKWI_URL!"=="" set "WOKWI_URL=%%u"
  )
)

if /i "%MODE%"=="--tanpa-sim" (
  echo Tanpa simulasi: menunggu data dari gateway ESP32.
  goto selesai
)
if /i "%MODE%"=="--lokal" goto lokal
if not "%WOKWI_URL%"=="" (
  start "" "%WOKWI_URL%"
  echo Wokwi    : %WOKWI_URL%  ^(tekan tombol Play hijau^)
  goto selesai
)
echo wokwi\wokwi-url.txt belum diisi -^> memakai simulator lokal. Lihat README bagian Wokwi.
:lokal
start "Simulator" cmd /k node simulator.mjs

:selesai
echo.
echo Semua berjalan. Jendela ini boleh ditutup.
timeout /t 5 >nul
