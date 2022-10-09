/*
    Demo for 13,56 MHz RFID reader/writer based on the popular (and cheap) RC522 reader
    module. This setup uses two LEDs to display a red/green status and features two
    switches to program cards for the red/green LEDs.
    JFTR: This is an incredibly crappy way to do card-base access control, DO NOT USE THIS
    APPROACH IN ANY LIVE PROJECT EVER!

    PINOUT for Arduino Nano:
    A4         -- SW2 (clear) to GND
    A5         -- SW1 (program) to GND
    D2         -- LED green to GND via 100R
    D4         -- LED red   to GND via 100R
    D9         -- RC522 RST
    D10 (SS)   -- RC522 SDA
    D11 (MISO) -- RC522 MISO
    D12 (MOSI) -- RC522 MOSI
    D13 (SCK)  -- RC522 SCK
    Add power supply rails accordingly.

*/

#include <SPI.h>
#include <MFRC522.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*
   Pin assignments
*/
const int PIN_SWITCH_1     = A5;
const int PIN_SWITCH_2     = A4;
const int PIN_LED_GREEN    = 2;
const int PIN_LED_RED      = 4;
const int PIN_RC522_RST    = 9;
const int PIN_RC522_SS     = 10;

/*
   Data block for "green" cards and addressing information
*/
const byte DATA_BLOCK_GREEN[] = {
  0x11, 0x21, 0x31, 0x41,
  0x12, 0x22, 0x32, 0x42,
  0x13, 0x23, 0x33, 0x43,
  0x14, 0x24, 0x34, 0x44
};
const int SECTOR = 1;
const int BLOCK_ADDRESS = 4;
const int TRAILER_BLOCK = 7;

/*
   Global objects and variables
*/
MFRC522 mfrc522(PIN_RC522_SS, PIN_RC522_RST);
MFRC522::MIFARE_Key key;

/*
   Initialization routines
*/

void setup() {
  Serial.begin(115200);

  // initialize discrete component pins
  pinMode(PIN_SWITCH_1, INPUT_PULLUP);
  pinMode(PIN_SWITCH_2, INPUT_PULLUP);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);

  // initialize RFID reader library
  SPI.begin();
  mfrc522.PCD_Init();

  // Prepare the key FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

/*
     Main processing loop
*/

void loop() {
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  int mode = ((!digitalRead(PIN_SWITCH_1) & 1) | ((!digitalRead(PIN_SWITCH_2) & 1) << 1));
  switch (mode) {
    case 0:
      loop_read_mode();
      break;
    case 1:
      loop_write_mode();
      break;
    case 2:
      loop_clear_mode();
      break;
    case 3:
      loop_read_continuous_mode();
      break;
  }
}

bool loop_card_available() {
  if (!mfrc522.PICC_IsNewCardPresent())
    return false;
  if (!mfrc522.PICC_ReadCardSerial())
    return false;

  // This process only works with MIFARE Classic cards.
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
          &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
          &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    return false;
  }

  return true;
}

void loop_clear_mode() {
  if (loop_card_available()) {
    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, TRAILER_BLOCK, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Clear mode PCD_Authenticate() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      digitalWrite(PIN_LED_RED, HIGH);
    } else {
      byte dataBlock[]    = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
      };
      status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(BLOCK_ADDRESS, dataBlock, 16);
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Clear mode MIFARE_Write() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        digitalWrite(PIN_LED_RED, HIGH);
      } else {
        digitalWrite(PIN_LED_GREEN, HIGH);
      }
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  delay(1000);
}

void loop_write_mode() {
  if (loop_card_available()) {
    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, TRAILER_BLOCK, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Write mode PCD_Authenticate() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      digitalWrite(PIN_LED_RED, HIGH);
    } else {
      status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(BLOCK_ADDRESS, DATA_BLOCK_GREEN, 16);
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Write mode MIFARE_Write() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        digitalWrite(PIN_LED_RED, HIGH);
      } else {
        digitalWrite(PIN_LED_GREEN, HIGH);
      }
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  delay(1000);
}

void loop_read_mode() {
  if (loop_card_available()) {
    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, TRAILER_BLOCK, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Read mode PCD_Authenticate() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      digitalWrite(PIN_LED_RED, HIGH);
    } else {
      byte buffer[18];
      byte size = sizeof(buffer);
      status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(BLOCK_ADDRESS, buffer, &size);
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Read Mode MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        digitalWrite(PIN_LED_RED, HIGH);
      } else {
        if (memcmp(buffer, DATA_BLOCK_GREEN, ARRAY_SIZE(DATA_BLOCK_GREEN)) == 0 ) {
          digitalWrite(PIN_LED_GREEN, HIGH);
        } else {
          digitalWrite(PIN_LED_RED, HIGH);
        }
      }
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  delay(500);
}

void loop_read_continuous_mode() {
  if (loop_card_available()) {
    MFRC522::StatusCode status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, TRAILER_BLOCK, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print(F("Read mode PCD_Authenticate() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      digitalWrite(PIN_LED_RED, HIGH);
    } else {
      byte buffer[18];
      byte size = sizeof(buffer);
      status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(BLOCK_ADDRESS, buffer, &size);
      while (status == MFRC522::STATUS_OK) {
        if (memcmp(buffer, DATA_BLOCK_GREEN, ARRAY_SIZE(DATA_BLOCK_GREEN)) == 0 ) {
          digitalWrite(PIN_LED_GREEN, HIGH);
        } else {
          digitalWrite(PIN_LED_RED, HIGH);
        }
        status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(BLOCK_ADDRESS, buffer, &size);
        delay(10);
      }
    }
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  delay(500);
}

/*
    Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte * buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
