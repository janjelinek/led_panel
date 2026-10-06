#include <Adafruit_NeoPixel.h>

// ESP32-C3: physical pin labelled 8 / D8. WS2812 DIN, shared ground.
constexpr uint8_t LED_PIN = 8;
constexpr uint16_t LED_COUNT = 256;
constexpr uint8_t MAX_LIT = 8;
constexpr uint8_t MAX_CHANNEL = 16;  // 6.3% maximum PWM, enforced before EVERY show.
constexpr bool SERPENTINE = true;
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
uint32_t frame = 0;
bool paused = false;

uint16_t xy(uint8_t x, uint8_t y) {
  return y * 16 + ((SERPENTINE && (y & 1)) ? 15 - x : x);
}

void dot(uint16_t index, uint16_t hue, uint8_t value = MAX_CHANNEL) {
  pixels.setPixelColor(index, pixels.ColorHSV(hue, 255, value));
}

// Hard frame guard: even a future effect cannot light more than eight pixels,
// or transmit a channel value above 16. No cumulative trails.
uint8_t safeShow() {
  uint8_t lit = 0;
  for (uint16_t i = 0; i < LED_COUNT; ++i) {
    uint32_t c = pixels.getPixelColor(i);
    if (!c) continue;
    if (lit >= MAX_LIT) { pixels.setPixelColor(i, 0); continue; }
    ++lit;
    uint8_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    pixels.setPixelColor(i, min(r, MAX_CHANNEL), min(g, MAX_CHANNEL), min(b, MAX_CHANNEL));
  }
  pixels.show();
  return lit;
}

void setup() {
  pixels.begin();
  pixels.clear();
  safeShow();
  Serial.begin(115200);
  // Never wait for a USB terminal: demo also runs after standalone power-up.
}

void loop() {
  while (Serial.available()) {
    char command = Serial.read();
    if (command == '0') paused = true;
    if (command == '1') paused = false;
  }
  pixels.clear();
  uint8_t effect = (frame / 250) % 3;  // About 12 seconds, including LED transmission.
  uint16_t hue = frame * 257;
  if (!paused) {
    if (effect == 0) {
      // Rainbow comet follows the physical LED chain; eight-pixel tapered tail.
      for (uint8_t i = 0; i < MAX_LIT; ++i)
        dot((frame + LED_COUNT - i) % LED_COUNT, hue + i * 3500, MAX_CHANNEL - i * 2);
    } else if (effect == 1) {
      // Four falling coloured droplets, each with one dim tail pixel.
      for (uint8_t i = 0; i < 4; ++i) {
        uint8_t x = 2 + i * 4, y = (frame / 2 + i * 5) % 16;
        dot(xy(x, y), hue + i * 16000);
        dot(xy(x, (y + 15) % 16), hue + i * 16000, 4);
      }
    } else {
      // Eight gently pulsing stars; move to a new arrangement every second.
      for (uint8_t i = 0; i < MAX_LIT; ++i) {
        uint16_t index = ((frame / 25) * 37 + i * 31) % LED_COUNT;
        uint8_t phase = (frame + i * 3) % 24;
        uint8_t value = 3 + (phase < 12 ? phase : 23 - phase);
        dot(index, hue + i * 8000, value);
      }
    }
  }
  uint8_t lit = safeShow();
  if (frame % 25 == 0)
    Serial.printf("LED demo: effect=%u lit=%u max_channel=%u uptime=%lu paused=%u\n",
                  effect, lit, MAX_CHANNEL, (unsigned long)millis(), paused);
  ++frame;
  delay(40);
}
