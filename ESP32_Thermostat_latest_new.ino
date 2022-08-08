/*
  This software, the ideas and concepts is Copyright (c) David Bird 2020
  All rights to this software are reserved.
  It is prohibited to redistribute or reproduce of any part or all of the software contents in any form other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.
  5. You MUST include all of this copyright and permission notice ('as annotated') and this shall be included in all copies or substantial portions of the software
     and where the software use is visible to an end-user.
  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
  FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR
  A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OR
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://dsbird.org.uk
//################# FOR ESP32 & BME80
I added a few lines (haikus) since original code tends to freeze - Robert Puhan wildheart@poetic.com 
So additional functions added - javascript picsum backgrounds, two aditional relays, target humidity, readings for pressure, dewpoint, altitude, wifi signal, histeresys for humidity, 
calibration for temperature and humidity, javascript refreshing for all website elements (except graphs) - no more freezed up actions, fontawesome, login for website, internet world radios 
on website RADIO LEMON, secondary sensor in bottom slide box, fullscreen button and last but not least jquery javascript...
//#################
I just added a few cruicial things to be able to use this awesome peace of coding by David
*/
//################# LIBRARIES ################
#include <WiFi.h>                      // Built-in
#include <ESPmDNS.h>                   // Built-in
#include <SPIFFS.h>                    // Built-in
#include "ESPAsyncWebServer.h"         // https://github.com/me-no-dev/ESPAsyncWebServer/tree/63b5303880023f17e1bca517ac593d8a33955e94
#include "AsyncTCP.h"                  // https://github.com/me-no-dev/AsyncTCP
#include <Wire.h>
#include <Adafruit_Sensor.h>           // Adafruit sensor
#include <Adafruit_BME280.h>           // For BME280 support
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 sensor;                // I2C mode
//################ VARIABLES ###########################################

const char* ServerName = "thermostat"; // Connect to the server with http://hpserver.local/ e.g. if name = "myserver" use http://myserver.local/

#define SensorReadings  144            // maximum number of sensor readings, typically 144/day at 6-per-hour
#define NumOfSensors    2              // number of sensors (+1), set by the graphing section
#define NumOfEvents     4              // Number of events per-day, 4 is a practical limit
#define noRefresh       false          // Set auto refresh OFF
#define Refresh         true           // Set auto refresh ON
#define ON              true           // Set the Relay ON
#define OFF             false          // Set the Relay OFF
#define RelayPIN1        26              // Define the Relay Control pin
#define RelayPIN2        25
#define RelayPIN3        32
#define RelayPIN4        33
#define RelayReverse1    true          // Set to true for Relay that requires a signal LOW for ON
#define RelayReverse2    true
#define RelayReverse3    false
#define RelayReverse4    false
#define SensorAddress   0x76           // Use 0x77 for an Adafruit variant
#define simulating      OFF            // Switch OFF for actual sensor readings, ON for simulated random values

typedef struct {
  float Temp  = 0;
  byte  Fahr  = 0;
  float Humi  = 0;
  byte  Press = 0;
  byte  Dewp  = 0;
  byte  Alti  = 0;
} sensordatatype;

sensordatatype sensordata[NumOfSensors][SensorReadings];

struct settings {
  String DoW;                // Day of Week for the programmed event
  String Start[NumOfEvents]; // Start time
  String Stop[NumOfEvents];  // End time
  String Temp[NumOfEvents];  // Required temperature during the Start-End times
  String Humi[NumOfEvents];  // Required humidity during the Start-End times
};
double dewPointFast(double celsius, double humidity)
 {
   double a = 17.62;
   double b = 243.12;
   double temp = (a * celsius) / (b + celsius) + log(humidity * 0.01);
   double Td = (b * temp) / (a - temp);
   return Td;
 }

String       Received_Data[20];                  // TxId, RxId, MsgCnt, Temperature, Humidity, RelayState1, Incoming Msg, Msg Rssi, Msg SNR (10-fields are sent to this Rx)
String       SensorReading[NumOfSensors][7];     // 254 Sensors max. and 6 Parameters per sensor T, H, Relay-state. Maximum LoRa adress range is 255 - 1 for Server so 0 - 253
String       DataFile = "params.txt";            // Storage file name on flash
String       Time_str, DoW_str;                  // For Date and Time
settings     Timer[7];                           // Timer settings, 7-days of the week
int          SensorReadingPointer[NumOfSensors]; // Used for sensor data storage
float        Hysteresis_temp     = 0.2;               // Heating Hysteresis1 default value
float        Hysteresis_humi     = 5.0;               // Heating Hysteresis2 default value
float        Calibrate_temp     = -1.0;
float        Calibrate_humi     = -1.0;
const String legendColour   = "black";           // Only use HTML colour names
const String titleColour    = "purple";
const String backgrndColour = "white";
const String data1Colour    = "red";
const String data2Colour    = "orange";

//################ VARIABLES ################
const char* ssid       = "KUKI";             // WiFi SSID     replace with details for your local network
const char* password   = "podmilscakova";         // WiFi Password replace with details for your local network
const char* http_username = "sebo";
const char* http_password = "sebo";
const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
// Example time zones
//const char* Timezone = "MET-1METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";       // Central Europe
//const char* Timezone = "EST-2METDST,M3.5.0/01,M10.5.0/02"; // Most of Europe
//const char* Timezone = "EST5EDT,M3.2.0,M11.1.0";           // EST USA
//const char* Timezone = "CST6CDT,M3.2.0,M11.1.0";           // CST USA
//const char* Timezone = "MST7MDT,M4.1.0,M10.5.0";           // MST USA
//const char* Timezone = "NZST-12NZDT,M9.5.0,M4.1.0/3";      // Auckland
//const char* Timezone = "EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia
//const char* Timezone = "ACST-9:30ACDT,M10.1.0,M4.1.0/3":   // Australia

// System values
String sitetitle            = "Smart Thermostat";
float  Temperature          = 0;          // Variable for the current temperature
float  Fahrenheit           = 0;          // Variable for the current fahrenheit
float  Humidity             = 0;          // Variable for the current humidity
float  Pressure             = 0;          // Variable for the current pressure
float  Dewpoint             = 0;          // Variable for the current dewpoint
float  Altitude             = 0;          // Variable for the current altitude
float  TargetTemp           = 21;         // Default thermostat value for set temperature
float  TargetHumi           = 30;
int    FrostTemp            = 5;          // Default thermostat value for frost protection temperature
int    FrostHumi            = 1;
float  ManOverrideTemp      = 21;         // Manual override temperature
float  ManOverrideHumi      = 50;
float  MaxTemperature       = 50;         // Maximum temperature detection, switches off thermostat when reached
float  MaxHumidity          = 100;
bool   ManualOverride       = false;      // Manual override
int    EarlyStart           = 0;          // Default thermostat value for early start of heating
String RelayState1          = "ON";      // Current setting of the control/thermostat relay
String RelayState2          = "ON";
String RelayState3          = "OFF";
String RelayState4          = "OFF";
String TimerState1          = "ON";      // Current setting of the timer
String TimerState2          = "ON";
String Units                = "M";        // or Units = "I" for °F and 12:12pm time format

String webpage              = "";         // General purpose variable to hold HTML code for display
int    TimerCheckDuration   = 5000;       // Check for timer event every 10-seconds
int    LastReadingDuration  = 1;          // Add sensor reading every n-mins
int    LastTimerSwitchCheck = 0;          // Counter for last timer check
int    LastReadingCheck     = 0;          // Counter for last reading saved check
float  LastTemperature      = 0;          // Last temperature used for rogue reading detection
float  LastHumidity         = 0;
int    UnixTime             = 0;          // Time now (when updated) of the current time

AsyncWebServer server(5959); 
//#########################################################################################
void setup() {
  pinMode(RelayPIN3, OUTPUT);
  digitalWrite(RelayPIN3, HIGH);
  pinMode(RelayPIN4, OUTPUT);
  digitalWrite(RelayPIN4, HIGH);
  SetupSystem();                          // General system setup
  StartWiFi();                            // Start WiFi services
  SetupTime();                            // Start NTP clock services
  StartSPIFFS();                          // Start SPIFFS filing system
  Initialise_Array();                     // Initialise the array for storage and set some values
  RecoverSettings();                      // Recover settings from LittleFS
  SetupDeviceName(ServerName);            // Set logical device name
 // Set handler for '/'
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->redirect("/homepage");       // Go to home page
  });
   server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    Logout();
    request->send(200, "text/html", webpage);
  });
  server.on("/logged_out", HTTP_GET, [](AsyncWebServerRequest *request){
    Logged_out();
    request->send(200, "text/html", webpage);
  });
  // Set handler for '/homepage'
  server.on("/homepage", HTTP_GET, [](AsyncWebServerRequest * request) {
    Homepage();
    request->send(200, "text/html", webpage);
  });
  server.on("/relay/off", HTTP_GET   , [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "OFF");
    RelayState3= "OFF";
    if (RelayReverse3) {
      digitalWrite(RelayPIN3, LOW);
    }
    else
    {
      digitalWrite(RelayPIN3, HIGH);
    }
  });
   server.on("/relay/on", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain","ON");
    RelayState3= "ON";
    if (RelayReverse3) {
      digitalWrite(RelayPIN3, HIGH);
    }
    else
    {
      digitalWrite(RelayPIN3, LOW);
    }
  });
  
  server.on("/relay/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain","ok");
    digitalWrite(RelayPIN3, !digitalRead(RelayPIN3));
  });
  
  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(digitalRead(RelayPIN3)));
  });

server.on("/relay1/off", HTTP_GET   , [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "OFF");
    RelayState4= "OFF";
if (RelayReverse4) {
      digitalWrite(RelayPIN4, LOW);
    }
    else
    {
      digitalWrite(RelayPIN4, HIGH);
    }  
    });
   server.on("/relay1/on", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain","ON");
    RelayState4= "ON";
if (RelayReverse4) {
      digitalWrite(RelayPIN4, HIGH);
    }
    else
    {
      digitalWrite(RelayPIN4, LOW);
    }   
    });
  
  server.on("/relay1/toggle1", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain","ok");
    digitalWrite(RelayPIN4, !digitalRead(RelayPIN4));
  });
  
  server.on("/relay1", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(digitalRead(RelayPIN4)));
  });

  // Set handler for '/homepage'
  server.on("/homepage", HTTP_GET, [](AsyncWebServerRequest * request) {
    Homepage();
    request->send(200, "text/html", webpage);
  });
  // Set handler for '/graphs'
  server.on("/graphs", HTTP_GET, [](AsyncWebServerRequest * request)   {
    Graphs();
    request->send(200, "text/html", webpage);
  });
  // Set handler for '/timer'
  server.on("/timer", HTTP_GET, [](AsyncWebServerRequest * request) {
    TimerSet();
    request->send(200, "text/html", webpage);
  });
  
  // Set handler for '/setup'
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest * request) {
    Setup();
    request->send(200, "text/html", webpage);
  });
    // Set handler for '/handletimer' inputs
  server.on("/handletimer", HTTP_GET, [](AsyncWebServerRequest * request) {
    for (byte dow = 0; dow < 7; dow++) {
      for (byte p = 0; p < NumOfEvents; p++) {
        Timer[dow].Temp[p]  = request->arg(String(dow) + "." + String(p) + ".Temp");
        Timer[dow].Humi[p]  = request->arg(String(dow) + "." + String(p) + ".Humi");
        Timer[dow].Start[p] = request->arg(String(dow) + "." + String(p) + ".Start");
        Timer[dow].Stop[p]  = request->arg(String(dow) + "." + String(p) + ".Stop");
      }
    }
    SaveSettings();
    request->redirect("/homepage");                       // Go back to home page
  });
  // Set handler for '/handlesetup' inputs
  server.on("/handlesetup", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasArg("hysteresis_temp")) {
      String numArg = request->arg("hysteresis_temp");
      Hysteresis_temp = numArg.toFloat();
    }
    if (request->hasArg("hysteresis_humi")) {
      String numArg = request->arg("hysteresis_humi");
      Hysteresis_humi = numArg.toFloat();
    }
    if (request->hasArg("calibrate_temp")) {
      String numArg = request->arg("calibrate_temp");
      Calibrate_temp = numArg.toFloat();
    }
    if (request->hasArg("calibrate_humi")) {
      String numArg = request->arg("calibrate_humi");
      Calibrate_humi = numArg.toFloat();
    }
    if (request->hasArg("frosttemp")) {
      String numArg = request->arg("frosttemp");
      FrostTemp     = numArg.toFloat();
    }
    if (request->hasArg("frosthumi")) {
      String numArg = request->arg("frosthumi");
      FrostHumi     = numArg.toFloat();
    }
    if (request->hasArg("earlystart")) {
      String numArg = request->arg("earlystart");
      EarlyStart    = numArg.toInt();
    }
    if (request->hasArg("manualoverride")) {
      String stringArg = request->arg("manualoverride");
      if (stringArg == "ON") ManualOverride = true; else ManualOverride = false;
    }
    if (request->hasArg("manualoverridetemp")) {
      String numArg   = request->arg("manualoverridetemp");
      ManOverrideTemp = numArg.toFloat();
    }
    if (request->hasArg("manualoverridehumi")) {
      String numArg   = request->arg("manualoverridehumi");
      ManOverrideHumi = numArg.toFloat();
    }
    
    SaveSettings();
    request->redirect("/homepage");                       // Go back to home page
  });
  server.begin();
  StartSensor();
  ReadSensor();                                           // Get current sensor values
  for (int r = 0; r < SensorReadings; r++) {
    sensordata[1][r].Temp = Temperature;
    sensordata[1][r].Fahr = Fahrenheit;
    sensordata[1][r].Humi = Humidity;
    sensordata[1][r].Press = Pressure;
    sensordata[1][r].Dewp = Dewpoint;
    sensordata[1][r].Alti = Altitude;
  }
  ActuateHeating1(OFF);                                    // Switch heating OFF
  ActuateHeating2(OFF);
  ReadSensor();                                           // Get current sensor valuest sensor values
  LastTimerSwitchCheck = millis() + TimerCheckDuration;   // preload timer value with update duration
}
//#########################################################################################
void loop() {
  if ((millis() - LastTimerSwitchCheck) > TimerCheckDuration) {
    LastTimerSwitchCheck = millis();                      // Reset time
    ReadSensor();                                         // Get sensor readings, or get simulated values if 'simulated' is ON
    UpdateLocalTime();                                    // Updates Time UnixTime to 'now'
    CheckTimerEvent1();                                    // Check for schkTimerEvent2();
    CheckTimerEvent2();
  }
  if ((millis() - LastReadingCheck) > (LastReadingDuration * 60 * 1000)) {
    LastReadingCheck = millis();                          // Update reading record every ~n-mins e.g. 60,000uS = 1-min
    AssignSensorReadingsToArray();
  }
}
//#########################################################################################
void Homepage() {
  ReadSensor();
  append_HTML_header(Refresh);
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<style>";
  webpage += "  .diver {overflow:hidden;margin-left:0px;width: 100vw;height: 80vh;display: flex;justify-content: center;align-items: center;}";
  webpage += "td, table {text-align:center;}";
  webpage += "  </style>";
  webpage += "<div class='diver' id='sample'>";
  String TimeNow;
    TimeNow = ConvertUnixTime(UnixTime);
    for (byte dow = 0; dow < 7; dow++) {
    for (byte p = 0; p < NumOfEvents; p++) {
      if (String(dow) == DoW_str && (TimeNow >= Timer[dow].Start[p] && TimeNow < Timer[dow].Stop[p]))
      {
        TargetTemp = Timer[dow].Temp[p].toFloat();
      
    webpage += "<p class='centre' style='position:fixed;bottom:56px;left:40%;text-shadow:1px 1px #000000;color:limegreen;cursor:none;text-decoration:none;'>TIMER nr."+String(p+1)+" ON  "+ Timer[dow].DoW +" from: " + Timer[dow].Start[p] + " to: " + Timer[dow].Stop[p] + "</p>";
      }
    }
  }  
  webpage += "<div title='Temperature "+ String(Temperature, 2) +"&deg; Relay "+ String(RelayState1) + "' style='cursor:none;position:fixed;top:0px;right:0px;' class='numberCircle'><span class=" + String((RelayState1 == "ON" ? "'on'>" : "'off'>")) + String(Temperature, 2) + "&deg;</span></div>";
  webpage += "<div title='Humidity "+ String(Humidity, 2) + "&percnt; Relay "+ String(RelayState2) + "'style='cursor:none;text-decoration:none;position:fixed;top:0px;right:200px;' class='numberCircle'><span class=" + String((RelayState2 == "ON" ? "'on'>" : "'off'>")) + String(Humidity, 2) + "&percnt;</span></div>";
  webpage += "<table style='background:rgba(255,255,255,0.7);font-weight:900;' class='centre'>";
  webpage += "<col><col><col><col><col><col><col><col><col><col><col><col><col><col><col><col>";
  webpage += "<tr><td>Control</td>";
  webpage += "<td >" + Timer[0].DoW + "</td>";
  for (byte dow = 1; dow < 6; dow++) { // Heading line showing DoW
    webpage += "<td>" + Timer[dow].DoW + "</td>";
  }
  webpage += "<td>" + Timer[6].DoW + "</td>";
  webpage += "</tr>";
  for (byte p = 0; p < NumOfEvents; p++) {
    webpage += "<tr><td>Temperature &degC;</td>";
    webpage += "<td><input disabled='disabled' type='text' name='" + String(0) + "." + String(p) + ".Temp' value='"  + Timer[0].Temp[p]   + "' maxlength='5' size='6'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input disabled='disabled' type='text' name='" + String(dow) + "." + String(p) + ".Temp' value='"      + Timer[dow].Temp[p] + "' maxlength='5' size='5'></td>";
    }
    webpage += "<td><input disabled='disabled' type='text' name='" + String(6) + "." + String(p) + ".Temp' value='"  + Timer[6].Temp[p]   + "' maxlength='5' size='5'></td>";
    webpage += "</tr>";
    webpage += "<tr><td>Humidity &percnt;</td>";
    webpage += "<td><input disabled='disabled' type='text' name='" + String(0) + "." + String(p) + ".Humi' value='"  + Timer[0].Humi[p]   + "' maxlength='5' size='6'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input disabled='disabled' type='text' name='" + String(dow) + "." + String(p) + ".Humi' value='"      + Timer[dow].Humi[p] + "' maxlength='5' size='5'></td>";
    }
    webpage += "<td class='large'><input disabled='disabled' type='text' name='" + String(6) + "." + String(p) + ".Humi' value='"  + Timer[6].Humi[p]   + "' maxlength='5' size='5'></td>";
    webpage += "</tr>";
    webpage += "<tr><td>Start</td>";
    webpage += "<td><input disabled='disabled' type='time' name='" + String(0) + "." + String(p) + ".Start' value='" + Timer[0].Start[p] + "'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input disabled='disabled' type='time' name='" + String(dow) + "." + String(p) + ".Start' value='" + Timer[dow].Start[p] + "'></td>";
    }
    webpage += "<td><input disabled='disabled' type='time' name='" + String(6) + "." + String(p) + ".Start' value='" + Timer[6].Start[p] + "'></td>";
    webpage += "</tr>";
    webpage += "<tr><td>Stop</td>";
    webpage += "<td><input disabled='disabled' type='time' name='" + String(0) + "." + String(p) + ".Stop' value='" + Timer[0].Stop[p] + "'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input disabled='disabled' type='time' name='" + String(dow) + "." + String(p) + ".Stop' value='" + Timer[dow].Stop[p] + "'></td>";
    }
    webpage += "<td><input disabled='disabled' type='time' name='" + String(6) + "." + String(p) + ".Stop' value='" + Timer[6].Stop[p] + "'></td>";
    webpage += "</tr>";
    if (p < (NumOfEvents - 1)) {
      webpage += "<tr><td></td><td>-</td>";
      for (int dow = 2; dow < 7; dow++) {
        webpage += "<td>-</td>";
      }
      webpage += "<td>-</td></tr>";
    }
  }
  webpage += "</table>";
  webpage += "<table style='z-index:-1;position:fixed;bottom:0px;width:100%;' class='centre'>";
  webpage += "<tr>";
  webpage += "<td class='largeTemp1'><i class='fas fa-temperature-high'></i></td>";
  webpage += "<td class='largeFahr1'><i class='fas fa-temperature-high'></i></td>";
  webpage += "<td class='largeHumi1'><i class='fas fa-water'></i></td>";
  webpage += "<td class='largePress1'><i class='fas fa-wind'></i></td>";
  webpage += "<td class='largeDewp1'><i class='fas fa-tint'></i></i></td>";
  webpage += "<td class='largeAlti1'><i class='fas fa-mountain'></i></td>";
  webpage += "<td class='large1'><i class='fas fa-wifi'></i></td>";
  webpage += "<td>Target <i style='color:red' class='fas fa-temperature-high'></i></td>";
  webpage += "<td>Target <i style='color:blue' class='fas fa-water'></i></td>";
  webpage += "<td>Status <i style='color:red' class='fas fa-temperature-high'></i></td>";
  webpage += "<td>Status <i style='color:blue' class='fas fa-water'></i></td>";
  webpage += "<td>Status &#128197;</td>";
  if (ManualOverride) {
    webpage += "<td>Override</td>";
  }
  webpage += "<td>Calibrate <i style='color:red;' class='fas fa-temperature-high'></td>";
  webpage += "<td>Calibrate <i style='color:blue;' class='fas fa-water'></td>"; 
  webpage += "<td><a style='text-decoration:none;' target='hidden' href='http://www.ihome.si:5959/relay/on'>I</a> <span class=" + String((RelayState3 == "ON" ? "'on'>" : "'off'>")) + "<i style='text-align:center;' class='fas fa-plug'></i></span> <a style='text-decoration:none;' target='hidden' href='http://www.ihome.si:5959/relay/off'>0</a></td>"; 
  webpage += "<td><a style='text-decoration:none;' target='hidden' href='http://www.ihome.si:5959/relay1/on'>I</a> <span class=" + String((RelayState4 == "ON" ? "'on'>" : "'off'>")) + "<i style='text-align:center;' class='fas fa-lightbulb'></i></span> <a style='text-decoration:none;' target='hidden' href='http://www.ihome.si:5959/relay1/off'>0</a></td>"; 
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td class='largeTemp'>" + String(Temperature, 1)       + "&deg;C</td>";
  webpage += "<td class='largeFahr'>" + String(Fahrenheit, 1)       + "&deg;F</td>";
  webpage += "<td class='largeHumi'>" + String(Humidity, 0)          + "%</td>";
  webpage += "<td class='largePress'>" + String(Pressure, 2)          + "hPa</td>";
  webpage += "<td class='largeDewp'>" + String(Dewpoint, 2)          + "&deg;C</td>";
  webpage += "<td class='largeAlti'>" + String(Altitude, 2)          + "m</td>";
  webpage += "<td class='largeWifi'><span>" + String(WiFiSignal()) + "</span></td>";
  webpage += "<td class='large'>" + String(TargetTemp + Calibrate_temp) + "&deg;</td>";
  webpage += "<td class='large'>" + String(TargetHumi + Calibrate_humi) + "&percnt;</td>";
  webpage += "<td class='large'><span class=" + String((RelayState1 == "ON" ? "'on'>" : "'off'>")) + RelayState1 + "</span></td>"; // (condition ? that : this) if this then that else this
  webpage += "<td class='large'><span class=" + String((RelayState2 == "ON" ? "'on'>" : "'off'>")) + RelayState2 + "</span></td>"; // (condition ? that : this) if this then that else this
  webpage += "<td class='large'><span class=" + String((TimerState1 == "ON" ? "'on'>" : "'off'>")) + TimerState1 + "</span></td>";
  if (ManualOverride) {
    webpage += "<td class='large'>" + String(ManualOverride ? "ON" : "OFF") + "</td>";
  }
  webpage += "<td class='largeTemp'>" + String(Calibrate_temp, 2) + "&deg;</td>";
  webpage += "<td class='largeHumi'>" + String(Calibrate_humi, 2) + "&percnt;</td>";
  webpage += "<td class='large'><div id='signal'><span class=" + String((RelayState3 == "ON" ? "'on'>" : "'off'>")) + RelayState3 + "</span></div></td>";
  webpage += "<td class='large'><div id='signal'><span class=" + String((RelayState4 == "ON" ? "'on'>" : "'off'>")) + RelayState4 + "</span></div></td>";
  webpage += "</tr>";
  webpage += "</table>";
  webpage += "</div>";
  webpage += "<script>var auto_refresh = setInterval( function () { $('#sample').load(' #sample > *'); }, 5000); auto_refresh = null;</script>";
  webpage += "<iframe name='hidden' style='width:0px;height:0px;visibility:hidden;'></iframe>";
  append_HTML_footer();
}
//#########################################################################################
void Logged_out() {
  append_HTML_header1(Refresh);
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<h2>Close your browser to complete logout process.</h2>";
  append_HTML_footer1();
}
//#########################################################################################
void Logout() {
  append_HTML_header1(Refresh);
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<h2>Close your browser to complete logout process.</h2>";
  append_HTML_footer1();
}
//#########################################################################################
void Graphs() {
  append_HTML_header(Refresh);
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>";
  webpage += "<script type='text/javascript'>";
  webpage += "google.charts.load('current', {'packages':['corechart']});";
  webpage += "google.charts.setOnLoadCallback(drawGraphT1);"; // Pre-load function names for Temperature graphs
  webpage += "google.charts.setOnLoadCallback(drawGraphH1);"; // Pre-load function names for Humidity graphs
  AddGraph(1, "GraphT", "Temperature", "TS", "°C", "red",  "chart_div");
  AddGraph(1, "GraphH", "Humidity",    "HS", "%",  "blue", "chart_div");
  webpage += "</script>";
  webpage += "<div id='outer'>";
  webpage += "<table>";
  webpage += "<tr>";
  webpage += "<td><div id='chart_divTS1' style='width:50%'></div></td>";
  webpage += "<td><div id='chart_divHS1' style='width:50%'></div></td>";
  webpage += "</tr>";
  webpage += "</table>";
  webpage += "<br>";
  webpage += "<table>";
  webpage += "<tr>";
  webpage += "<td>Temperature<div id='signal'><span class=" + String((RelayState1 == "ON" ? "'on'>" : "'off'>")) + RelayState1 + "</span></div></div></td>";
  webpage += "<script>var auto_refresh = setInterval(function () {$('#signal').load(' #signal > *');}, 5000);auto_refresh = null;</script>";
  webpage += "<td>Humidity<div id='signal1'><span class=" + String((RelayState2 == "ON" ? "'on'>" : "'off'>")) + RelayState2 + "</span></div></td>";
  webpage += "<script>var auto_refresh = setInterval(function () {$('#signal1').load(' #signal1 > *');}, 5000);auto_refresh = null;</script>";
  webpage += "</tr>";
  webpage += "</table>";
  webpage += "";
  webpage += "</div>";
  append_HTML_footer();
}
//#########################################################################################
void TimerSet() {
  append_HTML_header(noRefresh);
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<FORM action='/handletimer'>";
  webpage += "<table class='centre'>";
  webpage += "<col><col><col><col><col><col><col><col>";
  webpage += "<tr><td>Control</td>";
  webpage += "<td>" + Timer[0].DoW + "</td>";
  for (byte dow = 1; dow < 6; dow++) { // Heading line showing DoW
    webpage += "<td>" + Timer[dow].DoW + "</td>";
  }
  webpage += "<td>" + Timer[6].DoW + "</td>";
  webpage += "</tr>";
  for (byte p = 0; p < NumOfEvents; p++) {
    webpage += "<tr><td>Temp&deg;</td>";
    webpage += "<td><input type='text' name='" + String(0) + "." + String(p) + ".Temp' value='"  + Timer[0].Temp[p]   + "' maxlength='5' size='6'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input type='text' name='" + String(dow) + "." + String(p) + ".Temp' value='"      + Timer[dow].Temp[p] + "' maxlength='5' size='5'></td>";
    }
    webpage += "<td><input type='text' name='" + String(6) + "." + String(p) + ".Temp' value='"  + Timer[6].Temp[p]   + "' maxlength='5' size='5'></td>";
    webpage += "</tr>";
    webpage += "<tr><td>Humi&percnt;</td>";
    webpage += "<td><input type='text' name='" + String(0) + "." + String(p) + ".Humi' value='"  + Timer[0].Humi[p]   + "' maxlength='5' size='6'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input type='text' name='" + String(dow) + "." + String(p) + ".Humi' value='"      + Timer[dow].Humi[p] + "' maxlength='5' size='5'></td>";
    }
    webpage += "<td><input type='text' name='" + String(6) + "." + String(p) + ".Humi' value='"  + Timer[6].Humi[p]   + "' maxlength='5' size='5'></td>";
    webpage += "</tr>";
    webpage += "<tr><td>Start</td>";
    webpage += "<td><input type='time' name='" + String(0) + "." + String(p) + ".Start' value='" + Timer[0].Start[p] + "'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input type='time' name='" + String(dow) + "." + String(p) + ".Start' value='" + Timer[dow].Start[p] + "'></td>";
    }
    webpage += "<td><input type='time' name='" + String(6) + "." + String(p) + ".Start' value='" + Timer[6].Start[p] + "'></td>";
    webpage += "</tr>";
    webpage += "<tr><td>Stop</td>";
    webpage += "<td><input type='time' name='" + String(0) + "." + String(p) + ".Stop' value='" + Timer[0].Stop[p] + "'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input type='time' name='" + String(dow) + "." + String(p) + ".Stop' value='" + Timer[dow].Stop[p] + "'></td>";
    }
    webpage += "<td><input type='time' name='" + String(6) + "." + String(p) + ".Stop' value='" + Timer[6].Stop[p] + "'></td>";
    webpage += "</tr>";
    if (p < (NumOfEvents - 1)) {
      webpage += "<tr><td></td><td></td>";
      for (int dow = 1; dow < 7; dow++) {
        webpage += "<td>-</td>";
      }
      webpage += "<td></td></tr>";
    }
  }
  webpage += "</table>";
  webpage += "<div class='centre'>";
  webpage += "<br><input type='submit' value='Enter'><br><br>";
  webpage += "</div></form>";
  append_HTML_footer();
}
//#########################################################################################
void Setup() {
  append_HTML_header(noRefresh);
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<br>";
  webpage += "<FORM action='/handlesetup'>";
  webpage += "<table class='centre'>";
  webpage += "<tr>";
  webpage += "<td>Setting</td><td>Value</td>";
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='hysteresis_temp'>Hysteresis Temperature value (e.g. 0 - 1.0&deg;) [N.N]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9][.][0-9]' name='hysteresis_temp' value='" + String(Hysteresis_temp, 1) + "'></td>"; // 0.0 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='hysteresis_humi'>Hysteresis Humidity value (e.g. 0 - 5.0&percnt;) [N.N]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9][.][0-9]' name='hysteresis_humi' value='" + String(Hysteresis_humi, 1) + "'></td>"; // 0.0 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='calibrate_temp'>Calibrate Temperature value (e.g. 0 - 1.0&deg;) [N.N]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='^-?[0-9]+(.[0-9]{1,2})?$' name='calibrate_temp' value='" + String(Calibrate_temp, 1) + "'></td>"; // 0.0 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='calibrate_humi'>Calibrate Humidity value (e.g. 0 - 1.0&percnt;) [N.N]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='^-?[0-9]+(.[0-9]{1,2})?$' name='calibrate_humi' value='" + String(Calibrate_humi, 1) + "'></td>"; // 0.0 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='frosttemp'>Frost Protection Temperature&deg; [NN]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='frosttemp' value='" + String(FrostTemp) + "'></td>"; // 00-99 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='frosthumi'>Frost Protection Humidity&percnt; [NN]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='frosthumi' value='" + String(FrostHumi) + "'></td>"; // 00-99 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='earlystart'>Early start duration (mins) [NN]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='earlystart' value='" + String(EarlyStart) + "'></td>"; // 00-99 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='manualoveride'>Manual heating over-ride </label></td>";
  webpage += "<td><select name='manualoverride'><option value='ON'>ON</option>";
  webpage += "<option selected value='OFF'>OFF</option></select></td>"; // ON/OFF
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='manualoverridetemp'>Manual Override Temperature&deg; </label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='manualoverridetemp' value='" + String(ManOverrideTemp, 0) + "'></td>"; // 00-99 valid input style
  webpage += "</tr>";
  webpage += "<td><label for='manualoverridehumi'>Manual Override Humidity&percnt; </label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='manualoverridehumi' value='" + String(ManOverrideHumi, 0) + "'></td>"; // 00-99 valid input style
  webpage += "</tr>";  webpage += "</table>";
  webpage += "<br><input type='submit' value='Enter'><br><br>";
  webpage += "</form>";
  append_HTML_footer();
}

String PreLoadChartData(byte Channel, String Type) {
  byte r = 0;
  String Data = "";
  do {
    if (Type == "Temperature") {
      Data += "[" + String(r) + "," + String(sensordata[Channel][r].Temp, 1) + "," + String(TargetTemp, 1) + "],";
    }
    else
    {
      Data += "[" + String(r) + "," + String(sensordata[Channel][r].Humi) + "],";
    }
    r++;
  } while (r < SensorReadings);
  Data += "]";
  return Data;
}
//#########################################################################################
void AddGraph(byte Channel, String Type, String Title, String GraphType, String Units, String Colour, String Div) {
  String Data = PreLoadChartData(Channel, Title);
  webpage += "function draw" + Type + String(Channel) + "() {";
  if (Type == "GraphT") {
    webpage += " var data = google.visualization.arrayToDataTable(" + String("[['Hour', 'Rm T°', 'Tgt T°'],") + Data + ");";
  }
  else
    webpage += " var data = google.visualization.arrayToDataTable(" + String("[['Hour', 'RH %'],") + Data + ");";
  webpage += " var options = {";
  webpage += "  title: '" + Title + "',";
  webpage += "  titleFontSize: 14,";
  webpage += "  backgroundColor: '" + backgrndColour + "',";
  webpage += "  legendTextStyle: { color: '" + legendColour + "' },";
  webpage += "  titleTextStyle:  { color: '" + titleColour  + "' },";
  webpage += "  hAxis: {color: '#FFF'},";
  webpage += "  vAxis: {color: '#FFF', title: '" + Units + "'},";
  webpage += "  curveType: 'function',";
  webpage += "  pointSize: 1,";
  webpage += "  lineWidth: 1,";
  webpage += "  width:  450,";
  webpage += "  height: 280,";
  webpage += "  colors:['" + Colour + (Type == "GraphT" ? "', 'orange" : "") + "'],";
  webpage += "  legend: { position: 'right' }";
  webpage += " };";
  webpage += " var chart = new google.visualization.LineChart(document.getElementById('" + Div + GraphType + String(Channel) + "'));";
  webpage += "  chart.draw(data, options);";
  webpage += " };";
}
//#########################################################################################
void CheckTimerEvent1() {
  String TimeNow;
  UpdateTargetTemperature();
  TimeNow    = ConvertUnixTime(UnixTime);                  // Get the current time e.g. 15:35
  TimerState1 = "OFF";                                      // Switch timer off until decided by the schedule
  if (EarlyStart > 0) {                                    // If early start is enabled by a value > 0
    TimeNow = ConvertUnixTime(UnixTime + EarlyStart * 60); // Advance the clock by the Early Start Duration
  }
  if (ManualOverride == ON) {                              // If manual override is enabled then turn the heating on
    TargetTemp = ManOverrideTemp;                          // Set the target temperature to the manual overide temperature
    ControlHeating1();                                      // Control the heating as normal
  }
  else
  {
    for (byte dow = 0; dow < 7; dow++) {                     // Look for any valid timer events, if found turn the heating on
      for (byte p = 0; p < NumOfEvents; p++) {
        // Now check for a scheduled ON time, if so Switch the Timer ON and check the temperature against target temperature
        if (String(dow) == DoW_str && (TimeNow >= Timer[dow].Start[p] && TimeNow <= Timer[dow].Stop[p] && Timer[dow].Start[p] != ""))
        {
          TimerState1 = "ON";
          ControlHeating1();
          ManualOverride = OFF; // If it was ON turn it OFF when the timer starts a controlled period
        }
      }
    }
  }
  CheckAndSetFrostTemperature();
}
//#########################################################################################
void CheckTimerEvent2() {
  String TimeNow;
  UpdateTargetHumidity();
  TimeNow    = ConvertUnixTime(UnixTime);                  // Get the current time e.g. 15:35
  TimerState2 = "OFF";                                      // Switch timer off until decided by the schedule
  if (EarlyStart > 0) {                                    // If early start is enabled by a value > 0
    TimeNow = ConvertUnixTime(UnixTime + EarlyStart * 60); // Advance the clock by the Early Start Duration
  }
  if (ManualOverride == ON) {                              // If manual override is enabled then turn the heating on
    TargetHumi = ManOverrideHumi;                          // Set the target temperature to the manual overide temperature
    ControlHeating2();                                      // Control the heating as normal
  }
  else
  {
    for (byte dow = 0; dow < 7; dow++) {                     // Look for any valid timer events, if found turn the heating on
      for (byte p = 0; p < NumOfEvents; p++) {
        // Now check for a scheduled ON time, if so Switch the Timer ON and check the temperature against target temperature
        if (String(dow) == DoW_str && (TimeNow >= Timer[dow].Start[p] && TimeNow <= Timer[dow].Stop[p] && Timer[dow].Start[p] != ""))
        {
          TimerState2 = "ON";
          ControlHeating2();
          ManualOverride = OFF; // If it was ON turn it OFF when the timer starts a controlled period
        }
      }
    }
  }
  CheckAndSetFrostTemperature();
}
//#########################################################################################
void ControlHeating1() {
  if (Temperature < (TargetTemp - Hysteresis_temp + Calibrate_temp)) {           // Check if room temeperature is below set-point and hysteresis offset
    ActuateHeating1(ON);                                    // Switch Relay/Heating ON if so
  }
  if (Temperature > (TargetTemp + Hysteresis_temp + Calibrate_temp)) {           // Check if room temeperature is above set-point and hysteresis offset
    ActuateHeating1(OFF);                                   // Switch Relay/Heating OFF if so
  }
  if (Temperature > MaxTemperature) {                      // Check for faults/over-temperature
    ActuateHeating1(OFF);                                   // Switch Relay/Heating OFF if temperature is above maximum temperature
  }
}
//#########################################################################################
void ControlHeating2() {
  if (Humidity < (TargetHumi - Hysteresis_humi + Calibrate_humi)) {           // Check if room Humidity is below set-point and hysteresis offset
    ActuateHeating2(OFF);                                    // Switch Relay/Heating ON if so
  }
  if (Humidity > (TargetHumi + Hysteresis_humi + Calibrate_humi)) {           // Check if room Humidity is above set-point and hysteresis offset
    ActuateHeating2(ON);                                   // Switch Relay/Heating OFF if so
  }
  if (Humidity > MaxHumidity) {                      // Check for faults/over-Humidity
    ActuateHeating2(OFF);                                   // Switch Relay/Heating OFF if temperature is above maximum temperature
  }
}
//#########################################################################################
void UpdateTargetTemperature() {
  String TimeNow;
  TimeNow = ConvertUnixTime(UnixTime);
  for (byte dow = 0; dow < 7; dow++) {
    for (byte p = 0; p < NumOfEvents; p++) {
      if (String(dow) == DoW_str && (TimeNow >= Timer[dow].Start[p] && TimeNow < Timer[dow].Stop[p]))
      {
        TargetTemp = Timer[dow].Temp[p].toFloat();               // Found the programmed set-point temperature from the scheduled time period
        Serial.println("Period: " + String(p) + " " + String(p) + " "" from: " + Timer[dow].Start[p] + " to: " + Timer[dow].Stop[p]);
      }
    }
  }
  if (ManualOverride == ON) TargetTemp = ManOverrideTemp;
  Serial.println("Target Temperature = " + String(TargetTemp, 1) + "°");
}
//#########################################################################################
void UpdateTargetHumidity() {
  String TimeNow;
  TimeNow = ConvertUnixTime(UnixTime);
  for (byte dow = 0; dow < 7; dow++) {
    for (byte p = 0; p < NumOfEvents; p++) {
      if (String(dow) == DoW_str && (TimeNow >= Timer[dow].Start[p] && TimeNow < Timer[dow].Stop[p]))
      {
        TargetHumi = Timer[dow].Humi[p].toFloat();               // Found the programmed set-point temperature from the scheduled time period
      }
    }
  }
  if (ManualOverride == ON) TargetHumi = ManOverrideHumi;
  Serial.println("Target Humidity = " + String(TargetHumi, 1) + "°");
}
//#########################################################################################
void CheckAndSetFrostTemperature() {
  if (TimerState1 == "OFF" && ManualOverride == OFF) { // Only check for frost protection when heating is off
    if (Temperature < (FrostTemp - Hysteresis_temp + Calibrate_temp)) {     // Check if temperature is below Frost Protection temperature and hysteresis offset
      ActuateHeating1(ON);                             // Switch Relay/Heating ON if so
      Serial.println("Frost protection actuated...");
    }
    if (Temperature > (FrostTemp + Hysteresis_temp + Calibrate_temp)) {     // Check if temerature is above Frost Protection temperature and hysteresis offset
      ActuateHeating1(OFF);                            // Switch Relay/Heating OFF if so
    }
  }
}
//#########################################################################################
void CheckAndSetFrostHumidity() {
  if (TimerState2 == "OFF" && ManualOverride == OFF) { // Only check for frost protection when heating is off
    if (Humidity < (FrostHumi - Hysteresis_humi + Calibrate_humi)) {     // Check if temperature is below Frost Protection temperature and hysteresis offset
      ActuateHeating2(ON);                             // Switch Relay/Heating ON if so
      Serial.println("Frost protection actuated...");
    }
    if (Humidity > (FrostHumi + Hysteresis_humi + Calibrate_humi)) {     // Check if temerature is above Frost Protection temperature and hysteresis offset
      ActuateHeating2(OFF);                            // Switch Relay/Heating OFF if so
    }
  }
}
//#########################################################################################
void ActuateHeating1(bool demand) {
  pinMode(RelayPIN1, OUTPUT);
  if (demand) {
    RelayState1 = "ON";
    if (RelayReverse1) {
      digitalWrite(RelayPIN1, LOW);
    }
    else
    {
      digitalWrite(RelayPIN1, HIGH);
    }
    Serial.println("Thermostat ON");
  }
  else
  {
    RelayState1 = "OFF";
    if (RelayReverse1) {
      digitalWrite(RelayPIN1, HIGH);
    }
    else
    {
      digitalWrite(RelayPIN1, LOW);
    }
    Serial.println("Thermostat OFF");
    
  }
}
//#########################################################################################
void ActuateHeating2(bool demand) {
  pinMode(RelayPIN2, OUTPUT);
  if (demand) {
    RelayState2 = "ON";
    if (RelayReverse2) {
      digitalWrite(RelayPIN2, LOW);
    }
    else
    {
      digitalWrite(RelayPIN2, HIGH);
    }
    Serial.println("Thermostat2 ON");
  }
  else
  {
    RelayState2 = "OFF";
    if (RelayReverse2) {
      digitalWrite(RelayPIN2, HIGH);
    }
    else
    {
      digitalWrite(RelayPIN2, LOW);
    }
    Serial.println("Thermostat2 OFF");
    
  }
}
//#########################################################################################
void ReadSensor() {
  if (simulating) {
    Temperature = 20.2 + random(-15, 15) / 10.0; // Generate a random temperature value between 18.7° and 21.7°
    Humidity    = random(45, 55);                // Generate a random humidity value between 45% and 55%
  }
  else
  {
    while (isnan(sensor.readTemperature())) { }  // Make sure there are no reading errors
    Temperature = ((sensor.readTemperature()) + Calibrate_temp);      // Read the current temperature
    if (Temperature >= 50 || Temperature < -30) Temperature = LastTemperature; // Check and correct any errorneous readings
    LastTemperature = Temperature + Calibrate_temp;
    while (isnan(1.8 * sensor.readTemperature() + 32)) { }     // Make sure there are no reading errors
    Fahrenheit = 1.8 * sensor.readTemperature() + 32;
    while (isnan(sensor.readHumidity())) { }     // Make sure there are no reading errors
    Humidity = ((sensor.readHumidity()) + Calibrate_humi);
    if (Humidity >= 100 || Humidity < -30) Humidity = LastHumidity; // Check and correct any errorneous readings
    LastHumidity = Humidity + Calibrate_humi;
    while (isnan(sensor.readPressure() / 100.0F)) { }     // Make sure there are no reading errors
    Pressure = sensor.readPressure() / 100.0F;
    while (isnan(dewPointFast(sensor.readTemperature(),sensor.readHumidity()))) { }     // Make sure there are no reading errors
    Dewpoint = (dewPointFast(sensor.readTemperature(),sensor.readHumidity()));
    while (isnan(sensor.readAltitude(SEALEVELPRESSURE_HPA))) { }     // Make sure there are no reading errors
    Altitude = sensor.readAltitude(SEALEVELPRESSURE_HPA);
    Serial.println("Temperature = " + String(Temperature, 1) + ", Fahrenheit = " + String(Fahrenheit, 1) + ", Humidity = " + String(Humidity, 0) + ", Pressure = " + String(Pressure, 2) + ", Dewpoint = " + String(Dewpoint, 2) + " Altitude = " + String(Altitude, 2));
  }
}
//#########################################################################################
void AssignSensorReadingsToArray() {
  SensorReading[1][0] = 1;
  SensorReading[1][1] = Temperature;
  SensorReading[1][2] = Fahrenheit;
  SensorReading[1][3] = Humidity;
  SensorReading[1][4] = Pressure;
  SensorReading[1][5] = Dewpoint;
  SensorReading[1][6] = Altitude;
  SensorReading[1][7] = RelayState1;
  SensorReading[1][8] = RelayState2;
  AddReadingToSensorData(1, Temperature, Fahrenheit, Humidity, Pressure, Dewpoint, Altitude); // Only sensor-1 is implemented here, could  be more though
}
//#########################################################################################
void AddReadingToSensorData(byte RxdFromID, float Temperature, byte Fahrenheit, byte Humidity, byte Pressure, byte Dewpoint, byte Altitude) {
  byte ptr, p;
  ptr = SensorReadingPointer[RxdFromID];
  sensordata[RxdFromID][ptr].Temp = Temperature;
  sensordata[RxdFromID][ptr].Fahr = Fahrenheit;
  sensordata[RxdFromID][ptr].Humi = Humidity;
  sensordata[RxdFromID][ptr].Press = Pressure;
  sensordata[RxdFromID][ptr].Dewp = Dewpoint;
  sensordata[RxdFromID][ptr].Alti = Altitude;
  ptr++;
  if (ptr >= SensorReadings) {
    p = 0;
    do {
      sensordata[RxdFromID][p].Temp  = sensordata[RxdFromID][p + 1].Temp;
      sensordata[RxdFromID][p].Fahr  = sensordata[RxdFromID][p + 1].Fahr;
      sensordata[RxdFromID][p].Humi  = sensordata[RxdFromID][p + 1].Humi;
      sensordata[RxdFromID][p].Press  = sensordata[RxdFromID][p + 1].Press;
      sensordata[RxdFromID][p].Dewp  = sensordata[RxdFromID][p + 1].Dewp;
      sensordata[RxdFromID][p].Alti  = sensordata[RxdFromID][p + 1].Alti;
      p++;
    } while (p < SensorReadings);
    ptr = SensorReadings - 1;
    sensordata[RxdFromID][SensorReadings - 1].Temp = Temperature;
    sensordata[RxdFromID][SensorReadings - 1].Temp = Fahrenheit;
    sensordata[RxdFromID][SensorReadings - 1].Humi = Humidity;
    sensordata[RxdFromID][SensorReadings - 1].Press = Pressure;
    sensordata[RxdFromID][SensorReadings - 1].Dewp = Dewpoint;
    sensordata[RxdFromID][SensorReadings - 1].Alti = Altitude;
  }
  SensorReadingPointer[RxdFromID] = ptr;
}
//#########################################################################################
void append_HTML_header(bool refreshMode) {
  webpage  = "<!DOCTYPE html><html lang='en'>";
  webpage += "<head>";
  webpage += "<meta name=\"theme-color\" content=\"#000000\"><meta name=\"msapplication-navbutton-color\" content=\"#000000\"><meta name=\"apple-mobile-web-app-capable\" content=\"yes\"><meta name=\"apple-mobile-web-app-status-bar-style\" content=\"#000000\">";
  webpage += "<link rel='stylesheet' href='https://fonts.googleapis.com/css?family=Audiowide'>";
  webpage += "<link rel='icon' type='image/png' href='http://www.ihome.si:888/icon.png'>";
  webpage += "<title>" + sitetitle + "</title>";
  webpage += "<meta charset='UTF-8'>";
  webpage += "<link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css' integrity='sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr' crossorigin='anonymous'>";
  webpage += "<script src='https://code.jquery.com/jquery-3.2.1.min.js'></script>";
  webpage += "<style>";
  webpage += "body             {cursor:help;text-decoration:none;z-index: 59595959;overflow-x:hidden;font-family: 'Audiowide', sans-serif;width:100%;height:100%;margin-left:auto;text-shadow:1px 1px #fff;margin-right:auto;font-size:14px;color:black;background-color:transparent;text-align:center;}";
  webpage += ".centre          {z-index: 59595959;margin-left:auto;margin-right:auto;}";
  webpage += "h2               {z-index: 59595959;margin-top:0.3em;margin-bottom:0.3em;font-size:1.4em;}";
  webpage += "h3               {z-index: 59595959;margin-top:0.3em;margin-bottom:0.3em;font-size:1.2em;}";
  webpage += "h4               {z-index: 59595959;margin-top:0.3em;margin-bottom:0.3em;font-size:0.8em;}";
  webpage += ".on              {z-index: 59595959;color: limegreen;background:rgba(0,255,0,0.4);border-radius:50px;padding-left:5px;padding-right:5px;}";
  webpage += ".off             {z-index: 59595959;color: red;background:rgba(255,0,0,0.4);border-radius:50px;padding-left:5px;padding-right:5px;}";
  webpage += ".topnav          {z-index: 59595959;width:100%;position:fixed;margin-top:-10px;overflow: hidden;background-color: none;}";
  webpage += ".topnav a        {top:-10px;z-index: 59595959;float:left;color:black;text-align:center;padding:1em 1.14em;text-decoration:none;font-size:1.3em;}";
  webpage += ".topnav a:hover  {z-index: 59595959;background-color:deepskyblack;color:blue;}";
  webpage += ".topnav a.active {z-index: 59595959;background-color:lightblack;color:black;}";
  webpage += "table tr, td     {z-index: 59595959;padding:0.2em 0.5em 0.2em 0.5em;font-size:1.0em;font-family: 'Audiowide', sans-serif;}";
  webpage += "col:first-child  {z-index: 59595959;font-family: 'Audiowide', sans-serif;background:rgba(0,0,255,.5)}col:nth-child(2){background:#CCC}col:nth-child(8){background:#CCC}";
  webpage += "tr:first-child   {z-index: 59595959;font-family: 'Audiowide', sans-serif;background:rgba(0,0,255,.5)}";
  webpage += ".large           {z-index: 59595959;background:rgba(230,230,230,0.5);color:limegreen;font-size:1.8em;padding:0;margin:0}";
  webpage += ".large1          {z-index: 59595959;color:limegreen;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeTemp       {z-index: 59595959;background:rgba(230,230,230,0.5);color:red;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeFahr       {z-index: 59595959;background:rgba(230,230,230,0.5);color:yellow;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeHumi       {z-index: 59595959;background:rgba(230,230,230,0.5);color:blue;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largePress      {z-index: 59595959;background:rgba(230,230,230,0.5);color:grey;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeDewp       {z-index: 59595959;background:rgba(230,230,230,0.5);color:lightblue;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeAlti       {z-index: 59595959;background:rgba(230,230,230,0.5);color:limegreen;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeWifi       {z-index: 59595959;background:rgba(230,230,230,0.5);color:green;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeTemp1       {z-index: 59595959;color:red;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeFahr1       {z-index: 59595959;color:yellow;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeHumi1       {z-index: 59595959;color:blue;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largePress1      {z-index: 59595959;color:grey;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeDewp1       {z-index: 59595959;color:lightblue;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeAlti1       {z-index: 59595959;color:limegreen;font-size:1.8em;padding:0;margin:0}";
  webpage += ".largeWifi1       {z-index: 59595959;color:green;font-size:1.8em;padding:0;margin:0}";
  webpage += ".medium          {z-index: 59595959;font-size:1.4em;padding:0;margin:0}";
  webpage += ".ps              {z-index: 59595959;font-size:0.7em;padding:0;margin:0}";
  webpage += "#outer           {z-index: 59595959;width:100%;display:flex;justify-content:center;}";
  webpage += "footer           {z-index: 59595959;padding:0.08em;background-color:transparent;font-size:1.1em;}";
  webpage += ".numberCircle    {z-index: 59595959;;text-align:center;font-size:3em;";
  webpage += "                  z-index: 59595959;display:inline-flex;justify-content:center;align-items:center;}";
  webpage += ".wifi            {z-index: 59595959;padding:3px;position:relative;top:1em;left:0.36em;}";
  webpage += ".wifi, .wifi:before {z-index: 59595959;display:inline-block;border:9px double transparent;border-top-color:currentColor;border-radius:50%;}";
  webpage += ".wifi:before     {z-index: 59595959;content:'';width:0;height:0;}";
  webpage += "</style></head>";
  webpage += "<body style='z-index: 59595959;opacity: 1; transition: all 7s ease 7s;'>";
  webpage += "<script>var cb = 0;setInterval(function() {document.getElementsByTagName('body')[0].style.backgroundImage = 'url(https://picsum.photos/1920/1080/?random&cb=' + (++cb) + ')';}, 15000)</script>";
  webpage += "<div class='topnav'>";
  webpage += "<a title='Status' href='/'>Status</a>";
  webpage += "<a title='Graphs' href='graphs'>Graphs</a>";
  webpage += "<a title='Timers' href='timer'><div id='MyClockDisplay'><script>function showTime(){var date = new Date();var h = date.getHours();var m = date.getMinutes();var s = date.getSeconds();h = (h < 10) ? '0' + h : h;m = (m < 10) ? '0' + m : m;s = (s < 10) ? '0' + s : s;var time = h + ':' + m + ':' + s + ' ' ;document.getElementById('MyClockDisplay').innerText = time;document.getElementById('MyClockDisplay').textContent = time;setTimeout(showTime, 1000);}showTime();</script></div></a>";
  webpage += "<a title='Setup' href='setup'>Setup</a>";
  webpage += "<a title='Logout' style='cursor:pointer;' onclick='logoutButton()'>Logout</a>";
  webpage += "<script>function toggleCheckbox(element) {} function logoutButton() {  var xhr = new XMLHttpRequest();  xhr.open('GET', '/logout', true);  xhr.send();  setTimeout(function(){ window.open('/logged_out','_self'); }, 1000);}</script>";
  webpage += "</div>";
  webpage += "<style>.popup {position: fixed;top:700px;left:80px;display: inline-block;cursor: pointer;-webkit-user-select: none;-moz-user-select: none;-ms-user-select: none;user-select: none;z-index: 89898989;}.popup .popuptext {visibility: hidden;width: 365px;background-color: #ffffff;text-align: center;border-radius: 0px;position: absolute;z-index: 89898989;bottom: 125%;left: 50%;margin-left: -80px;padding:5px;border-radius:5px;}.popup .popuptext::after {content: '';position: absolute;top: 100%;left: 0%;margin-left: 0px;border:none;}.popup .show {visibility: visible;-webkit-animation: fadeIn 1s;animation: fadeIn 1s;}@-webkit-keyframes fadeIn {from {opacity: 0;} to {opacity: 1;}}@keyframes fadeIn {from {opacity: 0;}to {opacity:1 ;}}</style>";
  webpage += "<div class='popup' onclick='myPopup()'><i title='Radio' style='color:#ffffff;text-shadow:1px 1px #000;z-index:59595960;width:20px;height:20px;cursor:pointer;position:fixed;right:25px;top:105px;font:weight;font-size:20px;' class='fas fa-broadcast-tower'></i>";
  webpage += "<center><span class='popuptext' id='myPopup'><iframe style='text-align:center;' src='http://www.ihome.si:888/radio_files/rad.html' width='350px' height='600px' ></iframe></span></center>";
  webpage += "</div>";
  webpage += "<script>function myPopup() {var popup = document.getElementById('myPopup');popup.classList.toggle('show');}</script>";
  webpage += "<style>.myonoffswitch {--color-invert: rgba(0,255,0,.95);--width: 150px;--height: 150px;position: relative;display: flex;justify-content: center;align-items: center;width: var(--width);height: var(--height);transform: scale(0.25);transform-origin: 25% 0;}.myonoffswitch .button {width: 100%;height: 100%;display: flex;justify-content: center;align-items: center;position: relative;background: rgba(55,55,55,0.77);border-radius: 15px;}.myonoffswitch .button:after {content: '';width: 100%;height: 100%;position: absolute;background: radial-gradient(circle closest-side, var(--color-invert), transparent);filter: blur(20px);opacity: 0;transition: opacity 1s ease, transform 1s ease;transform: perspective(1px) translateZ(0);-webkit-backface-visibility: hidden;backface-visibility: hidden;}.myonoffswitch .button .power-on,.myonoffswitch .button .power-off {height: 100%;width: 100%;position: absolute;z-index: 1;fill: none;stroke: var(--color-invert);stroke-width: 8px;stroke-linecap: round;stroke-linejoin: round;}.myonoffswitch .button .power-on .line,.myonoffswitch .button .power-off .line {opacity: 0.2;}.myonoffswitch .button .power-on .circle,.myonoffswitch .button .power-off .circle {opacity: 0.2;transform: rotate(-58deg);transform-origin: center 80px;stroke-dasharray: 220;stroke-dashoffset: 40;}.myonoffswitch .button .power-on {filter: drop-shadow(0px 0px 6px rgba(255, 255, 255, 0.8));}.myonoffswitch .button .power-on .line {opacity: 0;transition: opacity 0.3s ease 1s;}.myonoffswitch .button .power-on .circle {opacity: 1;stroke-dashoffset: 220;transition: transform 0s ease, stroke-dashoffset 1s ease 0s;}.myonoffswitch input {position: absolute;height: 100%;width: 100%;z-index: 2;cursor: pointer;opacity: 0;}.myonoffswitch input:checked + .button:after {opacity: 0.15;transform: scale(2) perspective(1px) translateZ(0);-webkit-backface-visibility: hidden;backface-visibility: hidden;transition: opacity 0.5s ease, transform 0.5s ease;}.myonoffswitch input:checked + .button .power-on,.myonoffswitch input:checked + .button .power-off {-webkit-animation: click-animation 0.3s ease forwards;animation: click-animation 0.3s ease forwards;transform: scale(1);}.myonoffswitch input:checked + .button .power-on .line,.myonoffswitch input:checked + .button .power-off .line {-webkit-animation: line-animation 0.8s ease-in forwards;animation: line-animation 0.8s ease-in forwards;}.myonoffswitch input:checked + .button .power-on .circle,.myonoffswitch input:checked + .button .power-off .circle {transform: rotate(302deg);}.myonoffswitch input:checked + .button .power-on .line {opacity: 1;transition: opacity 0.05s ease-in 0.55s;}.myonoffswitch input:checked + .button .power-on .circle {transform: rotate(302deg);stroke-dashoffset: 40;transition: transform 0.4s ease 0.2s, stroke-dashoffset 0.4s ease 0.2s;}@-webkit-keyframes line-animation {0% {transform: translateY(0);}10% {transform: translateY(10px);}40% {transform: translateY(-25px);}60% {transform: translateY(-25px);}85% {transform: translateY(10px);}100% {transform: translateY(0px);}}@keyframes line-animation {0% {transform: translateY(0);}10% {transform: translateY(10px);}40% {transform: translateY(-25px);}60% {transform: translateY(-25px);}85% {transform: translateY(10px);}100% {transform: translateY(0px);}}@-webkit-keyframes click-animation {0% {transform: scale(1);}50% {transform: scale(0.9);}100% {transform: scale(1);}}@keyframes click-animation {0% {transform: scale(1);}50% {transform: scale(0.9);}100% {transform: scale(1);}}}</style>";
  webpage += "<div id='myonoffswitch' name='myonoffswitch' style='z-index:59595959;position:fixed;top:60px;right:-70px;' title='ON/OFF Fullscreen' class='myonoffswitch'><input onclick='fullscreen()' type='checkbox' /><div class='button'><svg id='power-off' class='power-off'><use xlink:href='#line' class='line' /><use xlink:href='#circle' class='circle' /></svg><svg id='power-on' class='power-on'><use xlink:href='#line' class='line' /><use xlink:href='#circle' class='circle' /></svg></div></div><svg xmlns='https://www.w3.org/2000/svg' style='display: none;'><symbol xmlns='https://www.w3.org/2000/svg' viewBox='0 0 150 150' id='line'><line x1='75' y1='34' x2='75' y2='58'/></symbol><symbol xmlns='https://www.w3.org/2000/svg' viewBox='0 0 150 150' id='circle'><circle cx='75' cy='80' r='35'/></symbol></svg>";
  webpage += "<script>function fullscreen() { if ((document.fullScreenElement && document.fullScreenElement !== null) || (!document.mozFullScreen && !document.webkitIsFullScreen)) { if (document.documentElement.requestFullScreen) { document.documentElement.requestFullScreen(); } else if (document.documentElement.mozRequestFullScreen) { document.documentElement.mozRequestFullScreen(); } else if (document.documentElement.webkitRequestFullScreen) { document.documentElement.webkitRequestFullScreen(Element.ALLOW_KEYBOARD_INPUT); } } else { if (document.cancelFullScreen) { document.cancelFullScreen(); } else if (document.mozCancelFullScreen) { document.mozCancelFullScreen(); } else if (document.webkitCancelFullScreen) { document.webkitCancelFullScreen(); } }}document.addEventListener('fullscreenchange', FSHandler);document.addEventListener('webkitfullscreenchange', FSHandler);document.addEventListener('mozfullscreenchange', FSHandler);document.addEventListener('MSFullscreenChange', FSHandler);function FSHandler() { if (!document.fullscreenElement && !document.webkitIsFullScreen && !document.mozFullScreen && !document.msFullscreenElement) { document.getElementById('power-switch').checked = false; } else { document.getElementById('power-switch').checked = true; }}</script>";
}
//#########################################################################################
void append_HTML_header1(bool refreshMode) {
  webpage  = "<!DOCTYPE html><html lang='en'>";
  webpage += "<head>";
  webpage  = "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  webpage += "<meta name=\"theme-color\" content=\"#000000\"><meta name=\"msapplication-navbutton-color\" content=\"#000000\"><meta name=\"apple-mobile-web-app-capable\" content=\"yes\"><meta name=\"apple-mobile-web-app-status-bar-style\" content=\"#000000\">";
  webpage += "<link rel='stylesheet' href='https://fonts.googleapis.com/css?family=Audiowide'>";
  webpage += "<link rel='icon' type='image/png' href='http://www.ihome.si:888/icon.png'>";
  webpage += "<title>" + sitetitle + "</title>";
  webpage += "<meta charset='UTF-8'>";
  webpage += "<link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css' integrity='sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr' crossorigin='anonymous'>";
  webpage += "<script src='https://code.jquery.com/jquery-3.2.1.min.js'></script>";
  webpage += "<style>";
  webpage += "body             {z-index: 59595959;overflow-x:hidden;font-family: 'Audiowide', sans-serif;width:100vw;margin-left:auto;text-shadow:1px 1px #fff;margin-right:auto;font-size:14px;color:black;background-color:transparent;text-align:center;}";
  webpage += ".centre          {z-index: 59595959;margin-left:auto;margin-right:auto;}";
  webpage += "h2               {z-index: 59595959;margin-top:0.3em;margin-bottom:0.3em;font-size:1.4em;}";
  webpage += ".large           {z-index: 59595959;background:rgba(230,230,230,0.5);color:limegreen;font-size:1.8em;padding:0;margin:0}";
  webpage += ".medium          {z-index: 59595959;font-size:1.4em;padding:0;margin:0}";
  webpage += ".ps              {z-index: 59595959;font-size:0.7em;padding:0;margin:0}";
  webpage += "#outer           {z-index: 59595959;width:100%;display:flex;justify-content:center;}";
  webpage += "footer           {z-index: 59595959;padding:0.08em;background-color:transparent;font-size:1.1em;}";
  webpage += "</style></head>";
  webpage += "<body style='z-index: 59595959;opacity: 1; transition: all 7s ease 7s;'>";
  webpage += "<script>var cb = 0; setInterval(function() { document.getElementsByTagName('body')[0].style.backgroundImage = 'url(https://picsum.photos/1920/1080/?random&cb=' + (++cb) + ')'; }, 15000)</script>";

}
//#########################################################################################
void append_HTML_footer() {
  webpage += "<footer>";
  webpage += "</footer>";
  webpage += "<style>.movingBox{  background:none;  width: 100%;  height: 35px;  position: fixed;  bottom: -37px;  left:0px;  transition: bottom 1s;  color:white;  text-shadow:1px 1px #000;}.clicked{  position: fixed;  bottom: 0px;  background:none;  color:white;  text-shadow:1px 1px #000;  cursor:pointer;overflow-x:scrolling;}button {  color: #900;  text-decoration: none;}</style>";
  webpage += "<div class='movingBox'>";
  webpage += "<div style='z-index:89898989;font-size:13px;text-shadow:1px 1px #ffffff;position:sticky;margin-bottom:-20px;left:50;'>";
  webpage += "</div><br>";
  webpage += "<button style='z-index:59595959;text-shadow:1px 1px #000;color:white;right:0px;font-weight:900;font-size:25px;background:none;border:none;cursor:pointer;align:center;bottom:110px;position:absolute;' onclick='function()'>";
  webpage += "<i title='Temperature Outside' class='fas fa-temperature-high'></i></button>";
  webpage += "<script>$(function(){ $('.movingBox').on('click',function(){ $(this).toggleClass('clicked'); });});</script>";
  webpage += "<center><iframe style='overflow:hidden;z-index:98989898;background:rgba(255,255,255,0.85);' scrolling='auto' name='iframe1' src='http://www.ihome.si:2345' height='100%' width='100%' frameborder='0'></iframe></center>";
  webpage += "</body></html>";
}
//#########################################################################################
void append_HTML_footer1() {
  webpage += "<footer>";
  webpage += "<div style='font-family: 'Audiowide', sans-serif;font-size:15px;text-shadow:1px 1px #ffffff;'>";
  webpage += "</div>";
  webpage += "</footer>";
  webpage += "</body></html>";
}
//#########################################################################################
void SetupDeviceName(const char *DeviceName) {
  if (MDNS.begin(DeviceName)) { // The name that will identify your device on the network
    Serial.println("mDNS responder started");
    Serial.print("Device name: ");
    Serial.println(DeviceName);
    MDNS.addService("n8i-mlp", "tcp", 23); // Add service
  }
  else
    Serial.println("Error setting up MDNS responder");
}
//#########################################################################################
void StartWiFi() {
  Serial.print("\r\nConnecting to: "); Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8); // Use Google as DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);       // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(50);
  }
  Serial.println("\nWiFi connected at: " + WiFi.localIP().toString());
}
//#########################################################################################
void SetupSystem() {
  Serial.begin(115200);                                           // Initialise serial communications
  delay(200);
  Serial.println(__FILE__);
  Serial.println("Starting...");
}
//#########################################################################################
boolean SetupTime() {
  configTime(0, 0, "time.nist.gov");                               // (gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                       // setenv()adds "TZ" variable to the environment, only used if set to 1, 0 means no change
  tzset();
  delay(200);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  char  time_output[30];
  while (!getLocalTime(&timeinfo, 15000)) {                        // Wait for up to 15-sec for time to synchronise
    return false;
  }
  time(&now);
  UnixTime = now;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  strftime(time_output, sizeof(time_output), "%H:%M", &timeinfo);  // Creates: '14:05'
  Time_str = time_output;
  strftime(time_output, sizeof(time_output), "%w", &timeinfo);     // Creates: '0' for Sun
  DoW_str  = time_output;
  return true;
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  strftime(output, sizeof(output), "%H:%M", now_tm);               // Returns 21:12
  return output;
}
//#########################################################################################
void StartSPIFFS() {
  Serial.println("Starting SPIFFS");
  boolean SPIFFS_Status;
  SPIFFS_Status = SPIFFS.begin();
  if (SPIFFS_Status == false)
  { // Most likely SPIFFS has not yet been formated, so do so
    Serial.println("Formatting SPIFFS (it may take some time)...");
    SPIFFS.begin(true); // Now format SPIFFS
    File datafile = SPIFFS.open("/" + DataFile, "r");
    if (!datafile || !datafile.isDirectory()) {
      Serial.println("SPIFFS failed to start..."); // Nothing more can be done, so delete and then create another file
      SPIFFS.remove("/" + DataFile); // The file is corrupted!!
      datafile.close();
    }
  }
  else Serial.println("SPIFFS Started successfully...");
}
//#########################################################################################
void Initialise_Array() {
  Timer[0].DoW = "Sunday"; Timer[1].DoW = "Monday"; Timer[2].DoW = "Tuesday"; Timer[3].DoW = "Wednesday"; Timer[4].DoW = "Thursday"; Timer[5].DoW = "Friday"; Timer[6].DoW = "Saturday";
}
//#########################################################################################
void SaveSettings() {
  Serial.println("Getting ready to Save settings...");
  File dataFile = SPIFFS.open("/" + DataFile, "w");
  if (dataFile) { // Save settings
    Serial.println("Saving settings...");
    for (byte dow = 0; dow < 7; dow++) {
      Serial.println("Day of week = " + String(dow));
      for (byte p = 0; p < NumOfEvents; p++) {
        dataFile.println(Timer[dow].Temp[p]);
        dataFile.println(Timer[dow].Humi[p]);
        dataFile.println(Timer[dow].Start[p]);
        dataFile.println(Timer[dow].Stop[p]);
        Serial.println("Period: " + String(p) + " " + Timer[dow].Temp[p] + " " + Timer[dow].Humi[p] +" from: " + Timer[dow].Start[p] + " to: " + Timer[dow].Stop[p]);
      }
    }
    dataFile.println(Hysteresis_temp, 1);
    dataFile.println(Hysteresis_humi, 1);
    dataFile.println(FrostTemp, 1);
    dataFile.println(EarlyStart);
    Serial.println("Saved Hysteresis Temperature : " + String(Hysteresis_temp));
    Serial.println("Saved Hysteresis Humidity : " + String(Hysteresis_humi));
    Serial.println("Saved Frost Temp : " + String(FrostTemp));
    Serial.println("Saved EarlyStart : " + String(EarlyStart));
    dataFile.close();
    Serial.println("Settings saved...");
  }
}
//#########################################################################################

void RecoverSettings() {
  String Entry;
  Serial.println("Reading settings...");
  File dataFile = SPIFFS.open("/" + DataFile, "r");
  if (dataFile) { // if the file is available, read it
    Serial.println("Recovering settings...");
    while (dataFile.available()) {
      for (byte dow = 0; dow < 7; dow++) {
        Serial.println("Day of week = " + String(dow));
        for (byte p = 0; p < NumOfEvents; p++) {
          Timer[dow].Temp[p]  = dataFile.readStringUntil('\n'); Timer[dow].Temp[p].trim();
          Timer[dow].Humi[p]  = dataFile.readStringUntil('\n'); Timer[dow].Humi[p].trim();
          Timer[dow].Start[p] = dataFile.readStringUntil('\n'); Timer[dow].Start[p].trim();
          Timer[dow].Stop[p]  = dataFile.readStringUntil('\n'); Timer[dow].Stop[p].trim();
          Serial.println("Period: " + String(p) + " " + Timer[dow].Temp[p] + " " + Timer[dow].Humi[p] +" from: " + Timer[dow].Start[p] + " to: " + Timer[dow].Stop[p]);
        }
      }
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); Hysteresis_temp = Entry.toFloat();
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); Hysteresis_humi = Entry.toFloat();
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); FrostTemp  = Entry.toInt();
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); EarlyStart = Entry.toInt();
      Serial.println("Recovered Hysteresis Temperature : " + String(Hysteresis_temp));
      Serial.println("Recovered Hysteresis Humidity : " + String(Hysteresis_humi));
      Serial.println("Recovered Frost Temp : " + String(FrostTemp));
      Serial.println("Recovered EarlyStart : " + String(EarlyStart));
      dataFile.close();
      Serial.println("Settings recovered...");
    }
  }
}
//#########################################################################################
void StartSensor() {
  Wire.setClock(100000);                           // Slow down the SPI bus for some BME280 devices
  if (!simulating) {                               // If not sensor simulating, then start the real one
    bool status = sensor.begin(SensorAddress);     // You can also pass a Wire library object like &Wire2, e.g. status = bme.begin(0x76, &Wire2);
    if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring, address and sensor ID!");
      Serial.print("SensorID is: 0x"); Serial.println(sensor.sensorID(), 16);
      Serial.print("       ID of 0xFF probably means a bad address, or a BMP 180 or BMP 085 or BMP280\n");
      Serial.print("  ID of 0x56-0x58 represents a BMP 280,\n");
      Serial.print("       ID of 0x60 represents a BME 280.\n");
    }
    else {
      Serial.println("Sensor started...");
    }
    delay(1000);                                   // Wait for sensor to start
  }
}
//#########################################################################################
String WiFiSignal() {
  float Signal = WiFi.RSSI();
  Signal = 90 / 40.0 * Signal + 212.5; // From Signal = 100% @ -50dBm and Signal = 10% @ -90dBm and y = mx + c
  if (Signal > 100) Signal = 100;
  return " " + String(Signal, 0) + "%";
}
