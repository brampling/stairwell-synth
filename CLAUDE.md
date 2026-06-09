# Spatial Instrument (working title)

A wireless, gesture-driven musical instrument that drives a multi-speaker spatial
audio array. A handheld controller reads sensors and sends control data over the air
to a fixed base, which renders spatial audio in SuperCollider on a Bela Gem Multi.
Rename this repo and heading once a real name is chosen.

## Architecture

Data path, left to right:

    sensors -> controller ESP32-S3 -> [ESP-NOW radio] -> receiver ESP32-S3
            -> [UART] -> Bela Gem Multi -> SuperCollider -> spatial audio out

- The controller is battery powered and mobile. It owns a short local I2C bus, reads
  the sensors, detects events, and transmits semantic data over ESP-NOW.
- The receiver is fixed beside the Bela and bridges ESP-NOW to the Bela over a serial
  link. It does no sensing of its own.
- The Bela runs the synth engine and the spatial decode, and outputs audio.

During early bring-up the real sensors are stood in for by a potentiometer (continuous
data) and a push switch (discrete event), so the transport can be proven before any
sensor hardware is involved.

## Repository layout

    /firmware          PlatformIO project, both ESP roles as separate environments
      platformio.ini
      /src
        /controller    controller role source
        /receiver      receiver bridge source
      /include
        protocol.h     shared packet definition, the single source of truth
    /bela              Bela render code and SuperCollider files
    /docs              pinouts, BOM, wiring notes, hardware gotchas
    /hardware          schematics and enclosure CAD (added later)
    CLAUDE.md

## Build and run

Firmware (runs locally on the Mac, built with PlatformIO):

- The official PlatformIO espressif32 platform lags the current Arduino-ESP32 core.
  Use the pioarduino fork in platformio.ini to get core 3.x with full S3 support.
- Build controller:   `pio run -e controller`
- Upload controller:  `pio run -e controller -t upload`
- Upload receiver:    `pio run -e receiver -t upload`
- Serial monitor:     `pio device monitor`

Bela (built on the board over Remote-SSH, not locally):

- Connect VS Code with Remote-SSH to the Gem, then edit and build there.
- Classic Bela run target is `make -C ~/Bela run PROJECT=<name>`. Confirm the exact
  target and project path against the Bela Gem docs, since the Gem image is newer.
- Keep the browser IDE open in a tab for the on-board scope and live console, which
  do not come through Remote-SSH.

## Hardware

- Controller and receiver: WaveShare WS24243, an ESP32-S3-DEV-KIT-N8R8 built on the
  ESP32-S3-WROOM-1-N8R8 module. Pin compatible with the Espressif ESP32-S3-DevKitC-1,
  so use the DevKitC-1 guide as the authoritative pinout.
- Base: Bela Gem Multi on PocketBeagle 2, 10 audio outputs, sub-millisecond latency.
- Transport: ESP-NOW (connectionless 2.4 GHz). Receiver to Bela is UART, 3.3 V both
  sides so no level shifter, with a shared ground.

## Pin assignments (verified safe on this board)

Controller and receiver ESP32-S3:

- Pot wiper:    GPIO4  (ADC1 channel 3; ADC1 stays usable with the radio active)
- Switch:       GPIO5  (internal pull-up, other leg to ground, debounced in firmware)
- UART to Bela: GPIO17 TX, GPIO18 RX  (leaves UART0 on GPIO43/44 free for USB)

Bela Gem Multi UART:

- UART4 on P1.20 (TX) and P2.20 (RX), device /dev/ttyS4, enabled via a device tree
  overlay. Zero-wiring alternative for early tests: plug the receiver into the Bela
  USB-A port and it enumerates as /dev/ttyUSB0.

Never use for I/O on this board:

- GPIO0, 3, 45, 46  (strapping pins)
- GPIO19, 20        (native USB)
- GPIO33 to 37      (tied to the octal PSRAM on the N8R8 variant)

## Conventions

- protocol.h is the single source of truth for the wire format. Any change to what the
  controller sends and what the Bela expects happens in the same commit. The two ends
  must never drift.
- Semantic data crosses the link, never raw signals. Debounce switches and detect
  events on the controller. For continuous values, send on a fixed tick or only on a
  threshold change, not every loop iteration.
- Writing in code comments, commit messages, and docs: no em dashes, rephrase instead.
  Use hyphens only for compound adjectives. Metric units. British English spelling.

## Gotchas

- ADC2 is unusable while the ESP-NOW radio is active. Analogue inputs must be on ADC1.
- macOS may not see the board if the WCH CH343 driver is missing. Install the CH34x VCP
  driver if no serial port appears on plug-in.
- A charge-only USB-C cable will not enumerate the board. Use a data cable.
- Manual download mode if an upload sticks: hold BOOT, tap RESET, release RESET, then
  release BOOT.
- ESP-NOW needs no library, just include esp_now.h. APA102 LEDs later use FastLED or
  Adafruit DotStar over hardware SPI.
- Bela outputs stay AC-coupled for normal audio. DC-coupling is a software setting only
  needed if driving CV out to external gear.

## Project status

Update this as stages complete.

- Stage 1, done: Bela Gem up, SuperCollider producing output.
- Stage 2, current: prove the ESP-NOW transport end to end with the pot and switch
  standing in, moving one SC parameter and firing one SC trigger.
- Stage 3: swap in real sensors (FDC1004 capacitive buttons, BNO085 IMU), add APA102
  LED feedback, use the bidirectional ESP-NOW link to echo synth state to the controller.
- Stage 4: build out the 10-speaker spatial array in the stairwell, with ambisonic or
  VBAP decode in SuperCollider.
