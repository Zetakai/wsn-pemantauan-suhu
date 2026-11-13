#!/usr/bin/env bash
# Satu klik: jalankan Node-RED, buka dashboard, lalu jalankan simulasi.
#
#   ./start.sh            Wokwi (jika wokwi/wokwi-url.txt berisi link proyek), selain itu simulator lokal
#   ./start.sh --lokal    selalu pakai simulator lokal (tanpa Wokwi)
#   ./start.sh --tanpa-sim  hanya Node-RED (data dari perangkat ESP32 nyata)
#   FLOW=flows-alternatif.json ./start.sh   pakai flow alternatif kelompok
#
# Tekan Ctrl+C untuk menghentikan semuanya.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
MODE="${1:-}"
PORT="${PORT:-1880}"
cd "$ROOT/node-red"

command -v node >/dev/null || { echo "Node.js belum terpasang. Unduh di https://nodejs.org (versi 20 atau lebih baru)."; exit 1; }

# Token Telegram dll. (opsional) dari file .env
if [ -f "$ROOT/.env" ]; then set -a; . "$ROOT/.env"; set +a; fi

if [ ! -d node_modules ]; then
  echo "Instalasi pertama: memasang Node-RED dan dashboard..."
  npm install --no-audit --no-fund
fi

buka() { (xdg-open "$1" || open "$1") >/dev/null 2>&1 || echo "Buka manual: $1"; }

PIDS=()
berhenti() { echo; echo "Menghentikan..."; kill "${PIDS[@]}" 2>/dev/null || true; wait 2>/dev/null || true; }
trap berhenti EXIT
trap 'exit 0' INT TERM

npx node-red --userDir . -p "$PORT" "${FLOW:-flows.json}" &
PIDS+=($!)

echo "Menunggu Node-RED di port $PORT..."
until node -e "fetch('http://localhost:$PORT/ui/').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"; do sleep 1; done
buka "http://localhost:$PORT/ui/"
echo "Dashboard : http://localhost:$PORT/ui/"
echo "Editor    : http://localhost:$PORT/"

WOKWI_URL=""
[ -f "$ROOT/wokwi/wokwi-url.txt" ] && WOKWI_URL="$(grep -v '^#' "$ROOT/wokwi/wokwi-url.txt" | grep -m1 -o 'https://wokwi.com/[^[:space:]]*' || true)"

case "$MODE" in
  --tanpa-sim) echo "Tanpa simulasi: menunggu data dari gateway ESP32." ;;
  --lokal) node simulator.mjs & PIDS+=($!) ;;
  *)
    if [ -n "$WOKWI_URL" ]; then
      buka "$WOKWI_URL"
      echo "Wokwi    : $WOKWI_URL  (tekan tombol Play hijau)"
    else
      echo "wokwi/wokwi-url.txt belum diisi -> memakai simulator lokal. Lihat README bagian Wokwi."
      node simulator.mjs & PIDS+=($!)
    fi
    ;;
esac

wait
