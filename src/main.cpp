#include <Arduino.h>
#include <vector>
#include "jsf.h"
#include "bytes_to_int.h"



enum class ControlCodes: uint8_t {
    Ack = 0x01,
    Retransmit = 0x03,
    Error = 0xEE,
    Reset = 0xFF
};


namespace {
    constexpr uint32_t seed{42}; // Probably could be part of payload but it's fine for now
    Jsf32 rng{seed};
    uint8_t receiveBuffer[9];  // Buffer for handshake data
    std::size_t frameLength{512};  // From handshake. Bytes per frame.
    uint32_t numFrames{0}; // Total number of frames to transmit
    bool useAck{false};  // Whether to wait for ACK per frame
    constexpr uint32_t kSendDelay{0}; // Optional delay between serial transmissions in microseconds
    constexpr uint32_t kACKTimeout{20000};  // Maximum time to wait for ACK in microseconds
    constexpr uint32_t kErrorDuration{10000}; // For blinking error indicator

    char errMsgBuf[64];
    std::size_t currentFrameNum{0};
    std::vector<uint32_t> frameBuffer(frameLength);
    bool waitingForHandshake{true};
} // namespace


void failLoop(const char* msg, uint32_t blinkInterval = 1000, uint32_t duration = 10000);

void failLoop(const char* msg, const uint32_t blinkInterval, const uint32_t duration) {
    bool ledState = false;
    Serial.println(msg);
    const auto start_time = millis();
    uint32_t elapsed = 0;
    while (elapsed < duration) {
        digitalWriteFast(LED_BUILTIN, ledState ? HIGH : LOW);
        ledState = !ledState;
        delay(blinkInterval);
        elapsed = millis() - start_time;
    }
}

void setupBuffer() {
    frameBuffer.clear();
    frameBuffer.resize(frameLength);
}


void fillFrameBuffer() {
    for (auto & i : frameBuffer) {
        i = rng();
    }
}


void handshake() {
    digitalWriteFast(LED_BUILTIN, HIGH);

    while (true) {
        if (const int avail = Serial.available(); static_cast<std::size_t>(avail) < sizeof(receiveBuffer)) {
            // not enough bytes yet
            delayMicroseconds(100);
        } else {
            break;
        }
    }
    delay(1);

    digitalWriteFast(LED_BUILTIN, LOW);

    bool failed{false};
    for (auto & i : receiveBuffer) {
        const auto data = Serial.read();
        if (data < 0) {
            failLoop("Handshake failed: invalid data received", 500);
            waitingForHandshake = true;
            failed = true;
        }
        // If failed, make sure to drain the serial buffer
        if (failed)
            continue;
        i = static_cast<uint8_t>(data);
    }
    if (failed) {
        std::fill_n(receiveBuffer, std::size(receiveBuffer), 0);
        return;
    }

    // We get 3 pieces of information from the handshake data:
    // [4B buffer size, 4B numSamples, 1B ackByte]
    const auto frameSize = bytes_to_int<uint32_t, ByteOrder::LittleEndian>(receiveBuffer);
    // Frame size is in bytes, so we need to convert it into number of samples for our case.
    frameLength = frameSize / sizeof(uint32_t);

    numFrames = bytes_to_int<uint32_t, ByteOrder::LittleEndian>(receiveBuffer + sizeof(uint32_t));
    if (const auto ackFlag = receiveBuffer[2*sizeof(uint32_t)]; ackFlag != 0x00) {
        useAck = true;
    } else {
        useAck = false;
    }

    // Clear the receive buffer
    std::fill_n(receiveBuffer, std::size(receiveBuffer), 0);

    // Setup the RNG and buffers
    rng.seed(seed);
    setupBuffer();
    fillFrameBuffer();

    waitingForHandshake = false;
    currentFrameNum = 0;

    delay(10); // Brief pause before starting transmission
}


void setup() {
    // put your setup code here, to run once:
    Serial.begin(8000000);
    while (!Serial && millis() < 3000) {
        // wait for USB Serial
    }
    // Serial.printf("Starting RNG transmission test. Seed: %u.\n", seed);

    pinMode(LED_BUILTIN, OUTPUT);
    waitingForHandshake = true;
    currentFrameNum = 0;

}

void transmit() {
    const size_t frameBufferSizeBytes = frameBuffer.size() * sizeof(frameBuffer[0]);
    const auto* bytes = reinterpret_cast<const uint8_t*>(frameBuffer.data());
    size_t written = 0;
    while (written < frameBufferSizeBytes) {
        written += Serial.write(bytes + written, frameBufferSizeBytes - written);
        Serial.send_now();
    }
}

void incrementSample () {
    fillFrameBuffer();
    currentFrameNum++;
}

void loop() {
    // put your main code here, to run repeatedly:


    if (waitingForHandshake) {
        handshake();
        return;
    }

    if (currentFrameNum < numFrames){
        transmit();
        if constexpr (kSendDelay > 0) {
            delayMicroseconds(kSendDelay);
        }

        if (useAck) {
            // wait for ACK
            uint32_t timeout{0};
            while (Serial.available() < 1) {
                delayMicroseconds(100);
                timeout += 100;
                if (timeout > kACKTimeout) {
                    failLoop("ACK timeout", 2000);
                    // Reset to handshake state
                    waitingForHandshake = true;
                    return;
                }
            }

            switch (const auto byte = static_cast<uint8_t>(Serial.read())) {
                case static_cast<uint8_t>(ControlCodes::Ack):
                    incrementSample();
                    break;
                case static_cast<uint8_t>(ControlCodes::Retransmit):
                    return;
                case static_cast<uint8_t>(ControlCodes::Reset):
                    waitingForHandshake = true;
                    return;
                case static_cast<uint8_t>(ControlCodes::Error):
                    snprintf(errMsgBuf, sizeof(errMsgBuf), "Received error byte: %u", byte);
                    failLoop(errMsgBuf, 3000, kErrorDuration);
                    waitingForHandshake = true;
                    return;
                default:
                    snprintf(errMsgBuf, sizeof(errMsgBuf), "Received unexpected byte: %u", byte);
                    failLoop(errMsgBuf, 250, kErrorDuration);
                    waitingForHandshake = true;
                    return;
            }
        } else {
            incrementSample();
        }

        if (currentFrameNum >= numFrames){
            waitingForHandshake = true;
        }
    }
}
