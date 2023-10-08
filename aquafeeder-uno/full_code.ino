#include <Arduino.h>
#include <U8x8lib.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <HX711_ADC.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#include <ArduinoJson.h>

#define DEBUG true

//Define variable for connection
//HX711 pins (weight):
const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin

//HX711 constructor (weight):
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;
unsigned long t = 0;
bool bol= 1;

// OLED display
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display

// servo motor
Servo myservo;

// ESP TX => Uno Pin 2
// ESP RX => Uno Pin 3
SoftwareSerial wifi(2, 3);

// **************
String sendDataToWiFiBoard(String command, const int timeout, boolean debug);
String prepareDataForWiFi(float weight);
void setup();
void loop();
// **************

/**
 * Build and return a JSON document from the sensor data
 * @param weight
 * @return
 */
String prepareDataForWiFi(float weight)
{
  StaticJsonDocument<200> doc;

  doc["weight"]    = String(weight);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  return jsonBuffer;
}

/**
 * Send data through Serial to ESP8266 module
 * @param command
 * @param timeout
 * @param debug
 * @return
 */
String sendDataToWiFiBoard(String command, const int timeout, boolean debug)
{
  String response = "";

  wifi.print(command); // send the read character to the esp8266

  long int time = millis();

  while((time+timeout) > millis()) {
    while(wifi.available()) {
      // The esp has data so display its output to the serial window
      char c = wifi.read(); // read the next character.
      response+=c;
    }
  }

  if (debug) {
    Serial.print(response);
  }

  return response;
}

// void setup
void setup() {
  wifi.begin(9600);
  // Weight measurement 
  Serial.begin(9600); delay(10);
  Serial.println();
  Serial.println("Starting...");

  LoadCell.begin();
  //LoadCell.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue; // calibration value (see example file "Calibration.ino")
  calibrationValue = 418.54; // uncomment this if you want to set the calibration value in the sketch
  // #if defined(ESP8266)|| defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266/ESP32 and want to fetch the calibration value from eeprom
  // #endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch the calibration value from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
    Serial.println("Startup is complete");
  }

  //for OLED display
  u8x8.begin();
  u8x8.setPowerSave(0);

  // //for motor, opening valve
  // pinMode(8,OUTPUT);

  // //server motor
  // myservo.attach(6,500,2400);
  
}

void loop() {

  if (DEBUG == true) {
    Serial.print("buffer: ");
    if (wifi.available()) {
      String espBuf;
      long int time = millis();

      while((time+1000) > millis()) {
        while (wifi.available()) {
          // The esp has data so display its output to the serial window
          char c = wifi.read(); // read the next character.
          espBuf += c;
        }
      }
      Serial.print(espBuf);
    }
    Serial.println(" endbuffer");
  }

  // read weight data 
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity
  char buf[6];
  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    float weight = LoadCell.getData();
    // Serial.print("Load_cell output val: ");
    // Serial.println(weight);

    // print weight on OLED
    u8x8.setCursor(2,1);
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.print("Weight=");
    u8x8.print(weight);
    u8x8.print("   ");

    // sending data to esp8266 module
    String preparedData = prepareDataForWiFi(weight);
    if (DEBUG == true) {
      Serial.println(preparedData);
    }
    sendDataToWiFiBoard(preparedData, 1000, DEBUG);
    delay(2000);
  }
}

  // //for motor spreading
  // digitalWrite(8,1);
  // delay(1000);
  // digitalWrite(8,0);
  
  // //server motor
  // myservo.write(0);
  // Serial.println("Servo 0:");
  // delay(2000);
  // myservo.write(180);
  // Serial.println("Servo 180:");
  // delay(2000);
