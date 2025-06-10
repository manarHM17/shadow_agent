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

# === Vérifier l'état des services ===
declare -A SERVICE_STATUS
for service in "${CHECK_SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$service" 2>/dev/null | tr -d '\n' || echo "unknown")
    SERVICE_STATUS["$service"]="$STATUS"
done

# === Détection des applications sous /opt au format app_name_version ===
declare -A APPLICATIONS
declare -A APP_VERSIONS

# Debug: Check if we can access /opt
if [[ ! -r /opt ]]; then
    echo "DEBUG: Cannot read /opt directory" >&2
fi

# Debug: List what we find in /opt
echo "DEBUG: Contents of /opt:" >&2
ls -la /opt 2>/dev/null || echo "DEBUG: Cannot list /opt contents" >&2

for dir in /opt/*; do
  echo "DEBUG: Processing: $dir" >&2
  
  # Check if the glob didn't match anything
  if [[ "$dir" == "/opt/*" ]]; then
    echo "DEBUG: No directories found in /opt" >&2
    break
  fi
  
  if [[ -d "$dir" ]]; then
    BASENAME=$(basename "$dir")
    echo "DEBUG: Found directory: $BASENAME" >&2
    # Remove trailing slash if any
    BASENAME="${BASENAME%/}"
    
    # Skip backup files
    if [[ "$BASENAME" == *.backup* ]]; then
      echo "DEBUG: Skipping backup file: $BASENAME" >&2
      continue
    fi
    
    # Check if directory name matches pattern: name_version where version contains numbers
    if [[ "$BASENAME" =~ ^(.+)_([0-9]+[a-zA-Z0-9._]*)$ ]]; then
      APP_NAME="${BASH_REMATCH[1]}"
      APP_VERSION="${BASH_REMATCH[2]}"
      echo "DEBUG: Detected versioned app: $APP_NAME version $APP_VERSION" >&2
      
      # If we already have this app, compare versions and keep the latest
      if [[ -n "${APPLICATIONS[$APP_NAME]}" ]]; then
        # Simple comparison: if new version is greater (lexicographically), use it
        if [[ "$APP_VERSION" > "${APPLICATIONS[$APP_NAME]}" ]]; then
          APPLICATIONS["$APP_NAME"]="$APP_VERSION"
          echo "DEBUG: Updated $APP_NAME to version $APP_VERSION" >&2
        fi
      else
        APPLICATIONS["$APP_NAME"]="$APP_VERSION"
      fi
    else
      echo "DEBUG: No version pattern found for: $BASENAME" >&2
      # If no version pattern found, treat as version-less application
      # Only add if it doesn't contain common system directories
      if [[ "$BASENAME" != "containerd" && "$BASENAME" != "google" && "$BASENAME" != "microsoft" ]]; then
        # Check if there's no versioned variant of this app already
        if [[ -z "${APPLICATIONS[$BASENAME]}" ]]; then
          APPLICATIONS["$BASENAME"]="latest"
          echo "DEBUG: Added version-less app: $BASENAME" >&2
        fi
      else
        echo "DEBUG: Skipping system directory: $BASENAME" >&2
      fi
    fi
  else
    echo "DEBUG: Not a directory: $dir" >&2
  fi
done

echo "DEBUG: Final applications array:" >&2
for app in "${!APPLICATIONS[@]}"; do
  echo "DEBUG: $app = ${APPLICATIONS[$app]}" >&2
done

# === Récupération des versions kernel, matériel et firmware ===
KERNEL_VERSION=$(uname -r)
# RPi specific hardware info from cpuinfo
HARDWARE_MODEL=$(grep "Model" /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//' || echo "unknown")
# RPi firmware version using vcgencmd (raspberry pi specific)
FIRMWARE_VERSION=$(vcgencmd version 2>/dev/null || echo "unknown")
# === Récupération de la version du système d'exploitation ===
OS_VERSION=$(grep '^PRETTY_NAME=' /etc/os-release | cut -d= -f2- | tr -d '"' || echo "unknown")

# === Création du fichier JSON pour les métriques matérielles ===
{
  echo "{"
  echo "  \"device_id\": \"$(<"$BASE_DIR/config/config.txt")\","
  echo "  \"readable_date\": \"$READABLE_DATE\","
  echo "  \"cpu_usage\": \"$CPU_USAGE\","
  echo "  \"memory_usage\": \"$MEMORY_USAGE_PERCENT\","
  echo "  \"disk_usage\": \"$DISK_USAGE\","
  echo "  $USB_DATA,"
  echo "  \"gpio_state\": $GPIO_STATE,"
  echo "  \"kernel_version\": \"$KERNEL_VERSION\","
  echo "  \"hardware_model\": \"$HARDWARE_MODEL\","
  echo "  \"firmware_version\": \"$FIRMWARE_VERSION\""
  echo "}"
} > "$LOG_DIR/$HARDWARE_FILE"

# === Création du fichier JSON pour les métriques logicielles/réseau ===
{
  echo "{"
  echo "  \"device_id\": \"$(<"$BASE_DIR/config/config.txt")\","
  echo "  \"readable_date\": \"$READABLE_DATE\","
  echo "  \"ip_address\": \"$IP_ADDRESS\","
  echo "  \"uptime\": \"$UPTIME\","
  echo "  \"os_version\": \"$OS_VERSION\","
  echo "  \"network_status\": \"$PING_STATUS\","
  echo -n "  \"applications\": "
  if [ ${#APPLICATIONS[@]} -eq 0 ]; then
    echo "{},"
  else
    echo "{"
    app_count=${#APPLICATIONS[@]}
    i=0
    for app_name in "${!APPLICATIONS[@]}"; do
      i=$((i+1))
      if [ $i -lt $app_count ]; then
        echo "    \"$app_name\": \"${APPLICATIONS[$app_name]}\","
      else
        echo "    \"$app_name\": \"${APPLICATIONS[$app_name]}\""
      fi
    done
    echo "  },"
  fi
  echo "  \"services\": {"
  service_count=${#SERVICE_STATUS[@]}
  i=0
  for s in "${!SERVICE_STATUS[@]}"; do
    i=$((i+1))
    if [ $i -lt $service_count ]; then
      echo "    \"$s\": \"${SERVICE_STATUS[$s]}\","
    else
      echo "    \"$s\": \"${SERVICE_STATUS[$s]}\""
    fi
  done
  echo "  }"
  echo "}"
} > "$LOG_DIR/$SOFTWARE_FILE"

# === Supprimer les fichiers JSON plus vieux de 10 minutes ===
find "$LOG_DIR" -name "*.json" -type f -mmin +10 -exec rm -f {} \;