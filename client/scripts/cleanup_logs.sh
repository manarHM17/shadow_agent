#!/bin/bash

# === Configuration ===
BASE_DIR="$(dirname "$(realpath "$0")")/.."
LOG_DIR="$BASE_DIR/logs"
MAX_AGE_HOURS=6

# === Nettoyage des fichiers plus vieux que MAX_AGE_HOURS ===
find "$LOG_DIR" -type f -name "*.json" -mmin +$((MAX_AGE_HOURS * 60)) -exec rm -f {} \;

echo "[✓] Nettoyage terminé : les fichiers JSON de plus de $MAX_AGE_HOURS heures ont été supprimés."
