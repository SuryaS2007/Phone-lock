#pragma once

// Classic 30-pin ESP32 / ESP-WROOM-32 DevKit V1 pin allocation.
namespace Pins {
constexpr int RFID_CS = 5;
constexpr int RFID_SCK = 18;
constexpr int RFID_MOSI = 23;
constexpr int RFID_MISO = 19;
constexpr int RFID_RST = 27;

constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;
constexpr int LEFT_CLAW_XSHUT = 32;
constexpr int RIGHT_CLAW_XSHUT = 33;

constexpr int DISPLAY_CLK = 14;
constexpr int DISPLAY_DIO = 13;

constexpr int LOCK_SERVO = 26;
constexpr int DISPENSER_SERVO = 25; // Reserved for the second SG90 in a later phase.
} // namespace Pins

namespace I2cAddresses {
constexpr unsigned char LEFT_CLAW = 0x30;
constexpr unsigned char RIGHT_CLAW = 0x31;
} // namespace I2cAddresses
