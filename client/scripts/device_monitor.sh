#!/bin/bash

# === Configurable Variables ===
BASE_DIR="$(dirname "$(realpath "$0")")/.."   # remonte depuis scripts vers client
LOG_DIR="$BASE_DIR/logs"
READABLE_DATE=$(date +"%Y-%m-%d %H:%M:%S")   # Date lisible
FILENAME="log_$READABLE_DATE.json"
CHECK_SERVICES=("ssh" "cron" "mosquitto")    # Services à surveiller
PING_TARGET="8.8.8.8"                        # IP à tester pour la connectivité réseau

# === Créer le dossier de logs s'il n'existe pas ===
mkdir -p "$LOG_DIR"

# === Récupération des informations système ===
CPU_USAGE=$(top -bn1 | grep "Cpu(s)" | awk '{print $2 + $4}') # Utilisation du CPU
CPU_USAGE="${CPU_USAGE}%"                                    # Ajouter le symbole %

MEMORY_TOTAL=$(free -m | awk '/Mem:/ {print $2}')             # Mémoire totale en Mo
MEMORY_USED=$(free -m | awk '/Mem:/ {print $3}')              # Mémoire utilisée en Mo
MEMORY_USAGE_PERCENT=$(awk "BEGIN {printf \"%.2f\", ($MEMORY_USED/$MEMORY_TOTAL)*100}") # Calculer le pourcentage
MEMORY_USAGE_PERCENT="${MEMORY_USAGE_PERCENT}%"              # Ajouter le symbole %

UPTIME=$(uptime -p)                                          # Temps de fonctionnement

DISK_USAGE=$(df -h / | awk 'NR==2 {print $5}')               # Utilisation du disque racine
# === Récupération des périphériques USB ===
USB_DEVICES=$(lsusb | awk -F 'ID ' '{print $2}' | jq -R -s -c 'split("\n")[:-1]')
IP_ADDRESS=$(hostname -I | awk '{print $1}')                 # Adresse IP

PING_STATUS=$(ping -c 1 $PING_TARGET > /dev/null 2>&1 && echo "reachable" || echo "unreachable")

# === Vérifier l’état des services ===
declare -A SERVICE_STATUS
for service in "${CHECK_SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$service" 2>/dev/null | tr -d '\n' || echo "unknown")
    SERVICE_STATUS["$service"]="$STATUS"
done

# === Création du fichier JSON ===
{
  echo "{"
  echo "  \"readable_date\": \"$READABLE_DATE\","
  echo "  \"cpu_usage\": \"$CPU_USAGE\","
  echo "  \"memory_usage\": \"$MEMORY_USAGE_PERCENT\","
  echo "  \"disk_usage_root\": \"$DISK_USAGE\","
  echo "  \"uptime\": \"$UPTIME\","
  echo "  \"usb_devices\": \"$USB_DEVICES\","
  echo "  \"ip_address\": \"$IP_ADDRESS\","
  echo "  \"network_status\": \"$PING_STATUS\","
  echo "  \"services\": {"
  for s in "${!SERVICE_STATUS[@]}"; do
    echo "    \"$s\": \"${SERVICE_STATUS[$s]}\","
  done | sed '$ s/,$//'  # Supprimer la virgule finale
  echo "  }"
  echo "}"
} > "$LOG_DIR/$FILENAME"

echo "[✓] Données système enregistrées dans : $FILENAME"