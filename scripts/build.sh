#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 scripts/embed_ui.py
ARDUINO_CLI='/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli'
"$ARDUINO_CLI" compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio --libraries ./libraries --build-path ./build-web ./led_web
