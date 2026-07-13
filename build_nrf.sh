#!/bin/bash
set -e

# Path to toolchain binary (use full path if not in PATH, otherwise default to system one)
OBJCOPY="arm-none-eabi-objcopy"
if ! command -v $OBJCOPY &> /dev/null; then
    OBJCOPY="/home/mohsin/gcc-arm-none-eabi-9-2020-q2-update/bin/arm-none-eabi-objcopy"
fi

PROJECT_DIR="/home/mohsin/contiki-ng/iot-project"

echo "=================================================="
echo "   Building Smart Health Firmware for nRF52840"
echo "=================================================="

# 1. Build Border Router
echo -e "\n🛠️ Building Border Router..."
cd "$PROJECT_DIR/cooja/border-router"
make clean TARGET=nrf52840 BOARD=dongle
make TARGET=nrf52840 BOARD=dongle -j$(nproc)
$OBJCOPY -O ihex build/nrf52840/dongle/border-router.nrf52840 build/nrf52840/dongle/border-router.hex
nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application-version 1 --application build/nrf52840/dongle/border-router.hex build/nrf52840/dongle/border-router.zip
echo "✅ Border Router ZIP created successfully."

# 2. Build Sensor Node (TinyML Patient Node)
echo -e "\n🛠️ Building Sensor Node (TinyML)..."
cd "$PROJECT_DIR/cooja/sensor-node"
make clean TARGET=nrf52840 BOARD=dongle
make TARGET=nrf52840 BOARD=dongle -j$(nproc)
$OBJCOPY -O ihex build/nrf52840/dongle/sensor-node.nrf52840 build/nrf52840/dongle/sensor-node.hex
nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application-version 1 --application build/nrf52840/dongle/sensor-node.hex build/nrf52840/dongle/sensor-node.zip
echo "✅ Sensor Node ZIP created successfully."

# 3. Build Spammer Node
echo -e "\n🛠️ Building Spammer Node..."
cd "$PROJECT_DIR/cooja/spammer"
make clean TARGET=nrf52840 BOARD=dongle
make TARGET=nrf52840 BOARD=dongle -j$(nproc)
$OBJCOPY -O ihex build/nrf52840/dongle/spammer.nrf52840 build/nrf52840/dongle/spammer.hex
nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application-version 1 --application build/nrf52840/dongle/spammer.hex build/nrf52840/dongle/spammer.zip
echo "✅ Spammer Node ZIP created successfully."

echo -e "\n🎉 All firmwares built and packaged successfully for nRF52840 Dongle PCA10059!"
echo "--------------------------------------------------"
echo "Generated ZIPs for flashing:"
echo "1. Border Router: $PROJECT_DIR/cooja/border-router/build/nrf52840/dongle/border-router.zip"
echo "2. Sensor Node  : $PROJECT_DIR/cooja/sensor-node/build/nrf52840/dongle/sensor-node.zip"
echo "3. Spammer Node  : $PROJECT_DIR/cooja/spammer/build/nrf52840/dongle/spammer.zip"
echo "=================================================="
