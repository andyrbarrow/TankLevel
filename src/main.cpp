/* A program for the M5Stick to take tank level from a resistive
tank level sensor and send it out using SignalK over UDP. It also
uses the M5Stck display to show level.

By Andy Barrow
GPL License applies
*/

#include <M5StickC.h>
#include <WiFi.h>
//this is version 5 of ArduinoJson. Some day I'll port to version 6 ....
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

#define ADC_INPUT 36 //pin 36
#define BEEPER 26 //pin 26
#define INTERNAL_LED 10 //internal led
#define SAMPLES 1000 //sample every second

esp_adc_cal_characteristics_t *adc_chars = new esp_adc_cal_characteristics_t;

WiFiUDP udp;

const char* ssid = "CASANET";
const char* password = "margaritaville";
IPAddress sigkserverip(10,10,10,1);
uint16_t sigkserverport = 55561;
int raw = 0; //this is the reading from the analog pin
int rawOld = 0; //this is the last reading of the analog pin
byte sendSig_Flag = 1;
//comment out the other tank
char* sensorKey = "tanks.freshWater.forwardTank.currentLevel";
//char* sensorKey = "tanks.freshWater.starboardTank.currentLevel";

// Function delcarations, needed for PlatformIO
void setup_wifi();
void clearscreen();
void testUDP();
void sendSigK(String sigKey, float data);
void reader();
void drawscale(float scaleLevel, float level);
int adjustLevel(int currentLevel);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting ");
  WiFi.begin(ssid, password);
  int reset_index = 0;
  clearscreen();
  M5.Lcd.println(" WiFi Connect");
  M5.Lcd.print(" ");
  M5.Lcd.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    M5.Lcd.print(".");
    //If WiFi doesn't connect in 60 seconds, do a software reset
    reset_index ++;
    if (reset_index > 60) {
      Serial.println("WIFI Failed - restarting");
      clearscreen();
      M5.Lcd.println(" WiFi Failed");
      M5.Lcd.println(" Restarting");
      delay(1000);
      ESP.restart();
    }
    if (WiFi.status() == WL_CONNECTED) {
      clearscreen();
      M5.Lcd.println(" WiFi");
      M5.Lcd.println(" Connected");
      M5.Lcd.println(WiFi.localIP());
      delay(500);
    }
  }
}

void clearscreen() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE ,BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0,0);
}

// send signalk data over UDP - thanks to PaddyB!
void sendSigK(String sigKey, float data)
{
 if (sendSig_Flag == 1)
 {
   DynamicJsonBuffer jsonBuffer;
   String deltaText;

   //  build delta message
   JsonObject &delta = jsonBuffer.createObject();

   //updated array
   JsonArray &updatesArr = delta.createNestedArray("updates");
   JsonObject &thisUpdate = updatesArr.createNestedObject();   //Json Object nested inside delta [...
   JsonArray &values = thisUpdate.createNestedArray("values"); // Values array nested in delta[ values....
   JsonObject &thisValue = values.createNestedObject();
   thisValue["path"] = sigKey;
   thisValue["value"] = data;
   thisUpdate["Source"] = "WaterSensors";

   // Send UDP packet
   udp.beginPacket(sigkserverip, sigkserverport);
   delta.printTo(udp);
   udp.println();
   udp.endPacket();
   delta.printTo(Serial);
   Serial.println();
 }
}

// Kludge to adjust for level guage that is non-linear
int adjustLevel(int currentLevel) {
  int newLevel;
  if (currentLevel < 15)
    newLevel = 0;
  else if (currentLevel < 35)
    newLevel = 5;
  else if (currentLevel < 50)
    newLevel = 15;
  else if (currentLevel < 58)
    newLevel = 23;
  else if (currentLevel < 68)
    newLevel = 30;
  else if (currentLevel < 78)
    newLevel = 45;
  else if (currentLevel < 85)
    newLevel = 60;
  else if (currentLevel < 90)
    newLevel = 70;
  else if (currentLevel < 94)
    newLevel = 78;
  else if (currentLevel < 98)
    newLevel = 85;
  else
    newLevel = 100;
 
  return newLevel;
}

void setup(void) {
  M5.begin();
  Serial.begin(115200);
  while (!Serial) continue;
  M5.Lcd.setRotation(1);
  //prep the onboard led, then turn it off
  pinMode(INTERNAL_LED, OUTPUT);
  digitalWrite(INTERNAL_LED, HIGH);
  //test sounder
  pinMode(BEEPER, OUTPUT);
  digitalWrite(BEEPER, LOW);
  setup_wifi();
  //sound beeper when wifi connects
  digitalWrite(BEEPER, HIGH);
  delay(500);
  digitalWrite(BEEPER, LOW);
  Serial.println("Setup Complete");
}

void drawscale(float scaleLevel, float level) {
  M5.Lcd.setRotation(4);
  M5.Lcd.fillRect(0,2,80,80,GREEN);
  M5.Lcd.fillRect(0,120,80,40,RED);
  M5.Lcd.fillRect(0,80,80,40,YELLOW);
  if ((int)scaleLevel < 150){
    M5.Lcd.fillRect(0,2,80,(int)scaleLevel,BLACK);
  }
  else {
    M5.Lcd.fillRect(0,2,80,(int)scaleLevel,RED);
  }
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(6,4);
  M5.Lcd.print((int)level);
  M5.Lcd.print("%");
}

void reader() {
    // Read the level
    // at the board, 0% full is 3.23V. 100% full is 1.72V
    // analog output 100% = 1969 (round to 1970) 0% = 4095
    // 3.3V--(level sensor 185 to 0 ohms)--GPIO36(ADC_INPUT)--/\/\/\/\(220ohms)--GND
    // 
    float level = 0;
    float meterLevel = 0;
    raw = analogRead(ADC_INPUT);
    Serial.print("analog read: ");
    Serial.println(raw);
    //level = ((float)raw / 1712) * 100;
    level = (1 - (((float)raw - 1970) / 2125)) * 100;
    // Adjust for non-linear guage
    level = adjustLevel(level);
    if ((raw/10) != (rawOld/10)) {
      rawOld = raw;
      Serial.print(raw);
      Serial.print(" ");
      Serial.println(rawOld);
      //The M5Stick screen is 160 px high, so we'll adjust to that.
      meterLevel = 160-(level*1.6);
      Serial.println(meterLevel);
      //If the level is zero, the screen will be completely black. We want to know
      //if we are actually at zero, so we'll turn the screen completely red
      if (level >= 100){
        level = 100;
        //turn on the led to show 100% full
        digitalWrite(INTERNAL_LED, LOW);
        digitalWrite(BEEPER, HIGH);
      }
      else {
        digitalWrite(INTERNAL_LED, HIGH);
        digitalWrite(BEEPER, LOW);
      }
      drawscale(meterLevel, level);
      Serial.println((int)level);
    }
    //little kludge to prevent temp and resistor variaions from making
    //level greater than 100%
    sendSigK(sensorKey, level/100); //send percent level via UDP
}

void loop(void) {
  reader();
  delay(500);
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }
}
