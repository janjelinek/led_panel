#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
ARDUINO_CLI='/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli'
"$ARDUINO_CLI" upload --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio --port "${1:-/dev/cu.usbmodem21301}" --input-dir ./build-web ./led_web
