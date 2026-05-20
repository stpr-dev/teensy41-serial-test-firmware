# teensy41-serial-test-firmware

Teensy 4.1 firmware for testing USB serial transfer reliability.

See [this blog post](https://stpr-dev.github.io/embedded/2026/05/19/teensy-usb-serial-max-speed/) for more information about why 
this repo exists. 

For companion Python script, [see this repository](https://github.com/stpr-dev/teensy41-serial-companion-python).

## Introduction

This project uses PlatformIO. It runs on a Teensy 4.1 and is designed to work with a companion program on a PC that initiates transfers and verifies data integrity.

The Teensy generates deterministic pseudo-random frames using a fixed-seed JSF32 RNG (seed = 42). Because the seed is fixed and known, the companion program can independently reproduce the expected byte sequence and check every received byte for correctness.

## Protocol

### Handshake (PC → Teensy, 9 bytes)

The companion program starts each test run by sending exactly 9 bytes:

| Bytes | Type          | Description                                          |
|-------|---------------|------------------------------------------------------|
| 0–3   | `uint32_t` LE | Frame size in **bytes** (must be a multiple of 4)    |
| 4–7   | `uint32_t` LE | Number of frames to transmit                         |
| 8     | `uint8_t`     | ACK mode: `0x00` = no ACK, any other value = use ACK |

The Teensy waits until all 9 bytes are available, then parses them and begins transmitting immediately (after a brief 10 ms settling delay).

### Data transfer (Teensy → PC)

Each frame consists of `(frame_size / 4)` pseudo-random `uint32_t` values, sent as raw little-endian bytes. Frames are transmitted sequentially until `num_frames` have been sent, after which the Teensy returns to the handshake-wait state.

### ACK mode (PC → Teensy, 1 byte per frame)

When ACK mode is enabled, the Teensy waits up to 20 ms after each frame for a single control byte from the PC before sending the next frame:

| Byte   | Name       | Teensy action                              |
|--------|------------|--------------------------------------------|
| `0x01` | Ack        | Advance to next frame                      |
| `0x03` | Retransmit | Resend the current frame                   |
| `0xFF` | Reset      | Abort and return to handshake-wait         |
| `0xEE` | Error      | Blink LED, abort, return to handshake-wait |

If no ACK arrives within 20 ms the Teensy blinks the LED for ~10 s and resets to handshake-wait.

When ACK mode is disabled, all frames are streamed back-to-back with no inter-frame delay.

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository and open it in PlatformIO.
3. Build and upload to your Teensy 4.1.
4. Run the companion program on your PC and connect to the Teensy's USB serial port.
5. Send the 9-byte handshake, then read and verify the incoming frames.
