// protocol.h - the single source of truth for the over-the-air wire format.
//
// Any change to what the controller sends and what the receiver decodes
// happens here, in the same commit as both ends. The two must never drift.
//
// This header defines the ESP-NOW packet only. The receiver translates each
// packet into a short ASCII line on the UART to the Bela (see receiver source
// and the SuperCollider patch), because that is far easier to parse in
// SuperCollider than a packed binary struct.

#pragma once

#include <stdint.h>

// Bump on any breaking change to ControlPacket. The receiver drops packets
// whose version it does not recognise, so a mismatched flash cannot inject
// garbage into the synth.
static const uint8_t PROTOCOL_VERSION = 1;

// Discrete one-shot events. Continuous data rides in the pot field instead.
enum EventType : uint8_t {
  EVENT_NONE        = 0,  // a plain continuous-value update, no event
  EVENT_SWITCH_DOWN = 1,  // switch pressed  (debounced on the controller)
  EVENT_SWITCH_UP   = 2,  // switch released
};

// Fixed 8-byte packet. Same compiler and architecture on both ends, but keep
// it packed so the layout is unambiguous if that ever stops being true.
typedef struct __attribute__((packed)) {
  uint8_t  version;  // == PROTOCOL_VERSION
  uint8_t  event;    // EventType
  uint16_t pot;      // continuous value, 0..4095 (ADC1). 0 until a pot exists.
  uint32_t seq;      // monotonic counter, lets the receiver spot dropped packets
} ControlPacket;
