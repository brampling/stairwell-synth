// Receiver role: fixed beside the Bela. Does no sensing of its own. It bridges
// ESP-NOW to the Bela over a UART link.
//
// Each valid packet becomes a short ASCII line on the UART, newline terminated,
// which SuperCollider parses trivially:
//   S1\n        switch down
//   S0\n        switch up
//   P<n>\n      pot value n, 0..4095
// The same lines are echoed to USB serial so you can watch the link without
// the Bela attached.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include "protocol.h"

// UART to the Bela, per CLAUDE.md. 3.3 V both sides, shared ground, no shifter.
static const int8_t PIN_UART_TX = 17;
static const int8_t PIN_UART_RX = 18;

// Must match the controller.
static const uint8_t RADIO_CHANNEL = 1;

HardwareSerial BelaSerial(1);  // UART1

// Track the last pot value forwarded so a static pot (or no pot at all, where
// the value stays 0) does not flood the link. The first packet still emits one
// P line, which is a handy "link is alive" marker during bring-up.
static uint16_t lastPotForwarded = 0xFFFF;
static bool     haveSeenPacket   = false;

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

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(ControlPacket)) return;

  ControlPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.version != PROTOCOL_VERSION) return;

  haveSeenPacket = true;

  switch (pkt.event) {
    case EVENT_SWITCH_DOWN:
      BelaSerial.println("S1");
      Serial.println("S1");
      break;
    case EVENT_SWITCH_UP:
      BelaSerial.println("S0");
      Serial.println("S0");
      break;
    default:
      break;
  }

  if (pkt.pot != lastPotForwarded) {
    lastPotForwarded = pkt.pot;
    BelaSerial.print("P");
    BelaSerial.println(pkt.pot);
    Serial.print("P");
    Serial.println(pkt.pot);
  }
}

void setup() {
  Serial.begin(115200);
  // HardwareSerial.begin order is (baud, config, rxPin, txPin).
  BelaSerial.begin(115200, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(RADIO_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);

  printMac("receiver MAC: ");
}

void loop() {
  // All work happens in the receive callback.
  delay(100);
}
