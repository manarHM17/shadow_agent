#!/bin/bash

# === Configurable Variables ===
BASE_DIR="$(dirname "$(realpath "$0")")/.."   # remonte depuis scripts vers client
LOG_DIR="$BASE_DIR/logs"

READABLE_DATE=$(date +"%Y-%m-%d_%H-%M-%S")   # Date lisible avec tirets pour compatibilité fichier
HARDWARE_FILE="hardware_metrics_$READABLE_DATE.json"
SOFTWARE_FILE="software_metrics_$READABLE_DATE.json"
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

# === USB Devices Detection ===
USB_DEVICES=$(lsusb)

if [ -z "$USB_DEVICES" ]; then
    USB_DATA="\"usb_state\": \"none\""
else
    # Escape newlines and quotes for JSON
    USB_DEVICES_CLEANED=$(echo "$USB_DEVICES" | sed ':a;N;$!ba;s/\n/ | /g' | sed 's/\"/\\"/g')
    USB_DATA="\"usb_state\": \"$USB_DEVICES_CLEANED\""
fi


# === GPIO State Count ===
GPIO_STATE=$(if command -v gpio >/dev/null 2>&1; then gpio readall | grep -c "ON"; else echo "0"; fi)

IP_ADDRESS=$(hostname -I | awk '{print $1}')                 # Adresse IP

PING_STATUS=$(ping -c 1 $PING_TARGET > /dev/null 2>&1 && echo "reachable" || echo "unreachable")

# === Vérifier l’état des services ===
declare -A SERVICE_STATUS
for service in "${CHECK_SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$service" 2>/dev/null | tr -d '\n' || echo "unknown")
    SERVICE_STATUS["$service"]="$STATUS"
done

# === Création du fichier JSON pour les métriques matérielles ===
{
  echo "{"
  echo "  \"readable_date\": \"$READABLE_DATE\","
  echo "  \"cpu_usage\": \"$CPU_USAGE\","
  echo "  \"memory_usage\": \"$MEMORY_USAGE_PERCENT\","
  echo "  \"disk_usage\": \"$DISK_USAGE\","
  echo "  $USB_DATA,"
  echo "  \"gpio_state\": $GPIO_STATE"
  echo "}"
} > "$LOG_DIR/$HARDWARE_FILE"

# === Création du fichier JSON pour les métriques logicielles/réseau ===
{
  echo "{"
  echo "  \"readable_date\": \"$READABLE_DATE\","
  echo "  \"ip_address\": \"$IP_ADDRESS\","
  echo "  \"uptime\": \"$UPTIME\","
  echo "  \"network_status\": \"$PING_STATUS\","
  echo "  \"services\": {"
  for s in "${!SERVICE_STATUS[@]}"; do
    echo "    \"$s\": \"${SERVICE_STATUS[$s]}\","
  done | sed '$ s/,$//' 
  echo "  }"
  echo "}"
} > "$LOG_DIR/$SOFTWARE_FILE"

# === Supprimer les fichiers JSON plus vieux de 20 minutes ===
find "$LOG_DIR" -name "*.json" -type f -mmin +10 -exec rm -f {} \;