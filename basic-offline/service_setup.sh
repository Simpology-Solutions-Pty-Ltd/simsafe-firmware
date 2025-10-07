#! /bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "Error: This script must be run as root"
  echo "Please run with: sudo $0"
  exit 1
fi

echo "Creating systemd service to run firmware"

CWD=$(pwd)
SERVICE_NAME="simsafe_firmware"
EXECUTABLE_PATH="${CWD}/main.out"
DESCRIPTION="SimSafe firmware for standalone lockers"

if [ ! -f "${EXECUTABLE_PATH}" ]; then
  echo "Error: Executable not found at ${EXECUTABLE_PATH}"
  exit 1
fi

if systemctl list-unit-files | grep -q "^${SERVICE_NAME}.service"; then
  echo "Service ${SERVICE_NAME} already exists. Removing..."
  systemctl stop ${SERVICE_NAME}.service 2>/dev/null
  systemctl disable ${SERVICE_NAME}.service 2>/dev/null
  rm -f /etc/systemd/system/${SERVICE_NAME}.service
  systemctl daemon-reload
  echo "Existing service removed."
fi

cat > /etc/systemd/system/${SERVICE_NAME}.service << EOF
[Unit]
Description=${DESCRIPTION}
After=network.target

[Service]
Type=simple
WorkingDirectory=${CWD}
ExecStart=${EXECUTABLE_PATH}
Restart=on-failure
RestartSec=5s
User=root

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload

systemctl enable ${SERVICE_NAME}.service

systemctl start ${SERVICE_NAME}.service

echo "Service created!"