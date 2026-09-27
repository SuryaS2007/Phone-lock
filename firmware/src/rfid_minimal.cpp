#include <Arduino.h>
#include <MFRC522.h>
#include <SPI.h>

namespace TestPins {
constexpr int RFID_CS = 5;
constexpr int RFID_SCK = 18;
constexpr int RFID_MOSI = 23;
constexpr int RFID_MISO = 19;
constexpr int RFID_RST = 27;
} // namespace TestPins

MFRC522 testReader(TestPins::RFID_CS, TestPins::RFID_RST);
unsigned long lastScanAt = 0;
unsigned long lastWaitingReportAt = 0;

String testUid() {
  String uid;
  for (byte index = 0; index < testReader.uid.size; ++index) {
    if (testReader.uid.uidByte[index] < 0x10) uid += '0';
    uid += String(testReader.uid.uidByte[index], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void printDigitalDiagnostics() {
  Serial.print("MINIMAL_RFID version_reads=");
  for (byte index = 0; index < 8; ++index) {
    if (index) Serial.print(',');
    Serial.printf("%02X", testReader.PCD_ReadRegister(MFRC522::VersionReg));
    delay(2);
  }
  Serial.println();

  testReader.PCD_SetAntennaGain(testReader.RxGain_min);
  const byte minimumGain = testReader.PCD_GetAntennaGain();
  testReader.PCD_SetAntennaGain(testReader.RxGain_max);
  const byte maximumGain = testReader.PCD_GetAntennaGain();
  const byte txControl = testReader.PCD_ReadRegister(MFRC522::TxControlReg);

  Serial.printf(
    "MINIMAL_RFID spi_hz=%lu gain_00=%02X gain_70=%02X round_trip=%s tx_control=%02X antenna_bits=%s self_test=NOT_RUN\n",
    static_cast<unsigned long>(MFRC522_SPICLOCK),
    minimumGain,
    maximumGain,
    minimumGain == testReader.RxGain_min && maximumGain == testReader.RxGain_max ? "PASS" : "FAIL",
    txControl,
    (txControl & 0x03) == 0x03 ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nMINIMAL_RFID boot: RC522 only; all other hardware disabled.");

  SPI.begin(TestPins::RFID_SCK, TestPins::RFID_MISO, TestPins::RFID_MOSI, TestPins::RFID_CS);
  testReader.PCD_Init();
  printDigitalDiagnostics();
}

void loop() {
  const unsigned long now = millis();
  if (now - lastScanAt < 250) return;
  lastScanAt = now;

  byte atqa[2];
  byte atqaSize = sizeof(atqa);
  const MFRC522::StatusCode wakeStatus = testReader.PICC_WakeupA(atqa, &atqaSize);
  if (wakeStatus != MFRC522::STATUS_OK && wakeStatus != MFRC522::STATUS_COLLISION) {
    if (now - lastWaitingReportAt >= 1000) {
      lastWaitingReportAt = now;
      Serial.printf("MINIMAL_RFID scan_status=%u (%s)\n",
        static_cast<unsigned int>(wakeStatus),
        testReader.GetStatusCodeName(wakeStatus));
    }
    return;
  }

  Serial.printf("MINIMAL_RFID atqa=%02X%02X wake_status=%u\n",
    atqa[0], atqa[1], static_cast<unsigned int>(wakeStatus));
  if (!testReader.PICC_ReadCardSerial()) {
    Serial.println("MINIMAL_RFID ATQA received but UID selection failed.");
    return;
  }

  Serial.printf("MINIMAL_RFID UID=%s size=%u sak=%02X\n",
    testUid().c_str(), testReader.uid.size, testReader.uid.sak);
  testReader.PICC_HaltA();
  testReader.PCD_StopCrypto1();
}
