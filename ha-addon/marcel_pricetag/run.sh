#!/usr/bin/env bash
set -e

CONFIG=/data/options.json
export REPO_URL="$(jq -r '.repo_url // "https://github.com/marcel7162/CC2510-ESL"' "$CONFIG" 2>/dev/null || echo https://github.com/marcel7162/CC2510-ESL)"
export BRANCH="$(jq -r '.branch // "main"' "$CONFIG" 2>/dev/null || echo main)"
export GATEWAY_HOST="$(jq -r '.gateway_host // "pricetag-ap.local"' "$CONFIG" 2>/dev/null || echo pricetag-ap.local)"
export WIFI_SSID="$(jq -r '.wifi_ssid // ""' "$CONFIG" 2>/dev/null || echo '')"
export WIFI_PASS="$(jq -r '.wifi_pass // ""' "$CONFIG" 2>/dev/null || echo '')"

echo "[addon] Marcel PriceTag startet"
echo "[addon] repo=$REPO_URL branch=$BRANCH gateway=$GATEWAY_HOST"

exec python3 /opt/app/server.py
