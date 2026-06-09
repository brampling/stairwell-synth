// Controller role: reads the local sensors, detects events, and transmits
// semantic data over ESP-NOW. Battery powered and mobile.
//
// Stage 2 bring-up: only the push switch on GPIO5 is wired. The pot is not
// connected yet, so pot handling is compiled out behind HAS_POT. Flip it to 1
// once a pot is on GPIO4 (ADC1) and rebuild. Nothing else needs to change, and
// the wire format already carries the field.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include "protocol.h"

// Set to 1 when a potentiometer wiper is connected to GPIO4.
#define HAS_POT 1

// Temporary bring-up aid: print the raw switch pin level a few times a second
// so wiring can be checked. Set back to 0 once the switch is proven.
#define SWITCH_DEBUG 0

// Pins per CLAUDE.md.
static const uint8_t PIN_SWITCH = 5;  // other leg to GND, internal pull-up, active low
static const uint8_t PIN_POT    = 4;  // ADC1 channel 3, stays usable with the radio active

// Fixed radio channel so the broadcast peers always agree. Both roles set this.
static const uint8_t RADIO_CHANNEL = 1;

// Broadcast during bring-up so no MAC pairing is needed. Narrow this to the
// receiver's MAC later if other 2.4 GHz traffic becomes a problem.
static uint8_t peerMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- switch debounce state ---
static const uint32_t DEBOUNCE_MS = 20;
static int      lastReading  = HIGH;
static int      stableState  = HIGH;
static uint32_t lastChangeMs = 0;

#if HAS_POT
// Continuous values go out on a fixed tick, and only when they actually move,
// never every loop iteration.
static const uint32_t POT_TICK_MS  = 30;
static const uint16_t POT_DEADBAND = 16;  // ignore jitter smaller than this
static uint32_t lastTickMs = 0;
static uint16_t lastPotSent = 0;
#endif

static uint16_t currentPot = 0;  // latest reading, 0 while no pot is wired
static uint32_t seq = 0;

// Read the station MAC straight from efuse so it is correct regardless of how
// far Wi-Fi has come up. WiFi.macAddress() can return zeros if called too early.
static void printMac(const char *label) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(label);
  Serial.println(buf);
}

static void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Broadcast frames report no ACK, so this is informational only.
}

static void sendPacket(uint8_t event, uint16_t pot) {
  ControlPacket pkt;
  pkt.version = PROTOCOL_VERSION;
  pkt.event   = event;
  pkt.pot     = pot;
  pkt.seq     = seq++;
  esp_now_send(peerMac, (const uint8_t *)&pkt, sizeof(pkt));
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SWITCH, INPUT_PULLUP);
  analogReadResolution(12);  // 0..4095, ready for the pot

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init failed");
    return;
  }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = RADIO_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  printMac("controller MAC: ");
}

void loop() {
  uint32_t now = millis();

#if SWITCH_DEBUG
  static uint32_t lastDbg = 0;
  if (now - lastDbg >= 250) {
    lastDbg = now;
    Serial.print("GPIO5 raw = ");
    Serial.println(digitalRead(PIN_SWITCH));  // 1 = HIGH (idle), 0 = LOW (pressed, if wired to GND)
  }
#endif

  // --- switch, debounced, edge-triggered ---
  int reading = digitalRead(PIN_SWITCH);
  if (reading != lastReading) {
    lastChangeMs = now;
    lastReading  = reading;
  }
  if ((now - lastChangeMs) > DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    // Active low: LOW means pressed.
    if (stableState == LOW) {
      sendPacket(EVENT_SWITCH_DOWN, currentPot);
      Serial.println("switch down");
    } else {
      sendPacket(EVENT_SWITCH_UP, currentPot);
      Serial.println("switch up");
    }
  }

#if HAS_POT
  // --- pot, on a tick, only when it moves ---
  if ((now - lastTickMs) >= POT_TICK_MS) {
    lastTickMs = now;
    currentPot = analogRead(PIN_POT);
    uint16_t delta = (currentPot > lastPotSent) ? (currentPot - lastPotSent)
                                                : (lastPotSent - currentPot);
    if (delta >= POT_DEADBAND) {
      lastPotSent = currentPot;
      sendPacket(EVENT_NONE, currentPot);
    }
  }
#endif
}
