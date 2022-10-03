/**
 *  Demo for 125 kHz RFID reader based on the popular (and cheap) RDM6300 reader
 *  module. This setup uses an OLED display to show the tag number.
 *  The RFID reader access code is based on the work of Michael Schoeffler, 
 *  available at http://www.mschoeffler.de.
 *
 *  PINOUT for Arduino Nano:
 *  A4 (SDA) -- OLED SDA
 *  A5 (SCL) -- OLED SCL
 *  D8 (TX)  -- RDM6300 RX 
 *  D9 (RX)  -- RDM6300 TX
 *  Add power supply rails accordingly.
 *  
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

/**
 * RFID pin assignments
 */
const int RFID_TX_PIN = 8;
const int RFID_RX_PIN = 9;
const int RFID_SPEED = 9600; // baud

/**
 * OLED settings
 */
const int SCREEN_WIDTH  = 128; // OLED display width, in pixels
const int SCREEN_HEIGHT =  64; // OLED display height, in pixels
const int OLED_RESET    =  -1; // Reset pin # (or -1 if sharing Arduino reset pin)

/**
 * RFID protocol handling stuff
 * 
 * The RFID data frame format is: 
 *    1 byte header (constant value: 2) 
 *   10 bytes data 
 *      2 bytes version
 *      8 bytes tag identifier 
 *    2 bytes checksum, 
 *    1 byte tail (constant value: 3)
 */
const int HEAD_SIZE = 1; 
const int DATA_VERSION_SIZE = 2; 
const int DATA_TAG_SIZE = 8; 
const int DATA_SIZE = DATA_VERSION_SIZE + DATA_TAG_SIZE; 
const int CHECKSUM_SIZE = 2;
const int TAIL_SIZE = 1; 
const int BUFFER_SIZE = HEAD_SIZE + DATA_SIZE + CHECKSUM_SIZE + TAIL_SIZE;

const uint8_t HEAD_VALUE = 2;
const uint8_t TAIL_VALUE = 3;

/**
 * RFID receiver buffer with index and some convenience pointers
 */
uint8_t buffer[BUFFER_SIZE]; 
int buffer_index = 0;
uint8_t *msg_head = buffer;
uint8_t *msg_data = buffer + HEAD_SIZE; 
uint8_t *msg_data_version = msg_data;
uint8_t *msg_data_tag = msg_data + DATA_VERSION_SIZE;
uint8_t *msg_checksum = msg_data + DATA_SIZE;
uint8_t *msg_tail = msg_checksum + CHECKSUM_SIZE;
    
/**
 * Other global objects and variables
 */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
int scanPos = 0;
SoftwareSerial ssrfid = SoftwareSerial(RFID_RX_PIN, RFID_TX_PIN); 

/**
 * Initialization routines
 */
 
void setup() {
  Serial.begin(115200);
  setup_display();
  setup_rfid();
}

void setup_display() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed, display not available"));
    for(;;); // Don't proceed, loop forever
  }
  // set default values
  display.setTextSize(2);  
  display.setTextColor(WHITE);       
  // blank screen
  display.clearDisplay();
  display.display();
}

void setup_rfid() {
  // init RFID connection
  ssrfid.begin(RFID_SPEED);
  ssrfid.listen(); 
}

/**
 * Main processing loop
 */

void loop() {
  display.clearDisplay();

  if (ssrfid.available() <= 0) {
    // no data available at the serial connection
    loop_display_idle_screen();
    delay(75);
  } else {
    bool call_extract_tag = false;

    // try to obtain data from the serial link
    int ssvalue = ssrfid.read(); 
    if (ssvalue == -1) { // no data was read
      return; // continue with next pass
    }
    
    if (ssvalue == HEAD_VALUE) { // RFID reader identified a tag, prepare for incoming data
      buffer_index = 0;
    } else if (ssvalue == TAIL_VALUE) { // tag data has been fully transmitted       
      call_extract_tag = true; // set flag to extract tag data
    }

    if (buffer_index >= BUFFER_SIZE) { // prevent against (unlikely) buffer overflow
      loop_display_buffer_overflow();
      delay(250);      
      return;
    }
    
    buffer[buffer_index++] = ssvalue; // add the byte received to the buffer

    if (call_extract_tag) {
      if (buffer_index == BUFFER_SIZE) {

        // extract the tag data
        unsigned long tag = hexstr_to_value(msg_data_tag, DATA_TAG_SIZE);

        // perform checksum check
        unsigned long checksum = 0;
        for (int i = 0; i < DATA_SIZE; i += CHECKSUM_SIZE) {
          long val = hexstr_to_value(msg_data + i, CHECKSUM_SIZE);
          checksum ^= val;
        }
        
        if (checksum == hexstr_to_value(msg_checksum, CHECKSUM_SIZE)) { // compare calculated checksum to retrieved checksum
          loop_display_tag_id(tag);
          delay(250);
        } else {
          loop_display_checksum_error();
          delay(250);      
        }

      } else { // buffer underrun
        // no error message because we might have accidentally intercepted the middle
        // of a conversation - just start again looking for header again
        buffer_index = 0;
        return;
      }
    }       
  }  
}

void loop_display_idle_screen() {
  display.setCursor(0, 0);        
  display.print(F("Scanning"));
  display.setCursor(scanPos * 5, 2 * 14);        
  display.print('*');
  scanPos = (scanPos > 20 ? 0 : scanPos + 1);
  display.display();
}

void loop_display_buffer_overflow() {
  display.setCursor(0, 0);        
  display.print(F("Buffer"));
  display.setCursor(0, 2 * 14);        
  display.print(F("Overflow"));
  display.display();
}

void loop_display_checksum_error() {
  display.setCursor(0, 0);        
  display.print(F("Checksum"));
  display.setCursor(0, 2 * 14);        
  display.print(F("Error"));
  display.display();
}

void loop_display_tag_id(unsigned long tag) {
  display.setCursor(0, 0);        
  display.print(F("Tag found"));
  display.setCursor(0, 2 * 14);        
  display.print(tag);
  display.display();
}

/**
 * Convert the hexadecimal character sequence of length length contained in str to an unsigned long value.
 */
unsigned long hexstr_to_value(char *str, unsigned int length) { 
  // create a null-terminated copy of the value
  char* copy = malloc((sizeof(char) * length) + 1); 
  memcpy(copy, str, sizeof(char) * length);
  copy[length] = '\0'; 
  // let strtol() do the heavy lifting
  unsigned long value = strtol(copy, NULL, 16);  
  free(copy); 
  return value;
}
