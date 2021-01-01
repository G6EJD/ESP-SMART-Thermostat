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
*/
//################# LIBRARIES ################
#include <WiFi.h>                      // Built-in
#include <ESPmDNS.h>                   // Built-in
#include <SPIFFS.h>                    // Built-in
#include "ESPAsyncWebServer.h"         // https://github.com/me-no-dev/ESPAsyncWebServer/tree/63b5303880023f17e1bca517ac593d8a33955e94
#include "AsyncTCP.h"                  // https://github.com/me-no-dev/AsyncTCP
#include <Adafruit_Sensor.h>           // Adafruit sensor
#include <Adafruit_BME280.h>           // For BME280 support
Adafruit_BME280 sensor;                // I2C mode

//################  VERSION  ###########################################
String version = "1.0";      // Programme version, see change log at end
//################ VARIABLES ###########################################

const char* ServerName = "thermostat"; // Connect to the server with http://hpserver.local/ e.g. if name = "myserver" use http://myserver.local/

#define SensorReadings  144            // maximum number of sensor readings, typically 144/day at 6-per-hour
#define NumOfSensors    2              // number of sensors (+1), set by the graphing section
#define NumOfEvents     4              // Number of events per-day, 4 is a practical limit
#define noRefresh       false          // Set auto refresh OFF
#define Refresh         true           // Set auto refresh ON
#define ON              true           // Set the Relay ON
#define OFF             false          // Set the Relay OFF
#define RelayPIN        0              // Define the Relay Control pin
#define LEDPIN          5              // Define the LED Control pin
#define RelayReverse    false          // Set to true for Relay that requires a signal LOW for ON

#define SensorAddress   0x76           // Use 0x77 for an Adafruit variant
#define simulating      OFF            // Switch OFF for actual sensor readings, ON for simulated random values

typedef struct {
  float Temp = 0;
  byte  Humi = 0;
} sensordatatype;

sensordatatype sensordata[NumOfSensors][SensorReadings];

struct settings {
  String DoW;                // Day of Week for the programmed event
  String Start[NumOfEvents]; // Start time
  String Stop[NumOfEvents];  // End time
  String Temp[NumOfEvents];  // Required temperature during the Start-End times
};

String       Received_Data[10];                  // TxId, RxId, MsgCnt, Temperature, Humidity, RelayState, Incoming Msg, Msg Rssi, Msg SNR (10-fields are sent to this Rx)
String       SensorReading[NumOfSensors][6];     // 254 Sensors max. and 6 Parameters per sensor T, H, Relay-state. Maximum LoRa adress range is 255 - 1 for Server so 0 - 253
String       DataFile = "params.txt";            // Storage file name on flash
String       Time_str, DoW_str;                  // For Date and Time
settings     Timer[7];                           // Timer settings, 7-days of the week
int          SensorReadingPointer[NumOfSensors]; // Used for sensor data storage
float        Hysteresis     = 0.2;               // Heating Hysteresis default value
const String legendColour   = "black";           // Only use HTML colour names
const String titleColour    = "purple";
const String backgrndColour = "gainsboro";
const String data1Colour    = "red";
const String data2Colour    = "orange";

//################ VARIABLES ################
const char* ssid       = "yourSSID";             // WiFi SSID     replace with details for your local network
const char* password   = "YourPASSWORD";         // WiFi Password replace with details for your local network
const char* Timezone   = "GMT0BST,M3.5.0/01,M10.5.0/02";
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
String Year                 = "2020";     // For the footer line
float  Temperature          = 0;          // Variable for the current temperature
float  Humidity             = 0;          // Variable for the current temperature

float  TargetTemp           = 21;         // Default thermostat value for set temperature
int    FrostTemp            = 5;          // Default thermostat value for frost protection temperature
float  ManOverrideTemp      = 21;         // Manual override temperature
float  MaxTemperature       = 28;         // Maximum temperature detection, switches off thermostat when reached

bool   ManualOverride       = false;      // Manual override
int    EarlyStart           = 0;          // Default thermostat value for early start of heating
String RelayState           = "OFF";      // Current setting of the control/thermostat relay
String TimerState           = "OFF";      // Current setting of the timer
String Units                = "M";        // or Units = "I" for °F and 12:12pm time format

String webpage              = "";         // General purpose variable to hold HTML code for display
int    TimerCheckDuration   = 5000;       // Check for timer event every 5-seconds
int    LastReadingDuration  = 1;          // Add sensor reading every n-mins
int    wifi_signal          = 0;          // WiFi signal strength
int    LastTimerSwitchCheck = 0;          // Counter for last timer check
int    LastReadingCheck     = 0;          // Counter for last reading saved check
float  LastTemperature      = 0;          // Last temperature used for rogue reading detection
int    UnixTime             = 0;          // Time now (when updated) of the current time

AsyncWebServer server(80); // Server on IP address port 80 (web-browser default, change to your requirements, e.g. 8080
// To access server from outside of a WiFi (LAN) network e.g. on port 8080 add a rule on your Router that forwards a connection request
// to http://your_WAN_address:8080/ to http://your_LAN_address:8080 and then you can view your ESP server from anywhere.
// Example http://yourhome.ip:8080 and your ESP Server is at 192.168.0.40, then the request will be directed to http://192.168.0.40:8080

//#########################################################################################
void setup() {
  SetupSystem();                          // General system setup
  StartWiFi();                            // Start WiFi services
  SetupTime();                            // Start NTP clock services
  StartSPIFFS();                          // Start SPIFFS filing system
  Initialise_Array();                     // Initialise the array for storage and set some values
  RecoverSettings();                      // Recover settings from LittleFS
  SetupDeviceName(ServerName);            // Set logical device name
  // Set handler for '/'
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->redirect("/homepage");       // Go to home page
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
  // Set handler for '/help'
  server.on("/help", HTTP_GET, [](AsyncWebServerRequest * request) {
    Help();
    request->send(200, "text/html", webpage);
  });
  // Set handler for '/handletimer' inputs
  server.on("/handletimer", HTTP_GET, [](AsyncWebServerRequest * request) {
    for (byte dow = 0; dow < 7; dow++) {
      for (byte p = 0; p < 4; p++) {
        Timer[dow].Temp[p]  = request->arg(String(dow) + "." + String(p) + ".Temp");
        Timer[dow].Start[p] = request->arg(String(dow) + "." + String(p) + ".Start");
        Timer[dow].Stop[p]  = request->arg(String(dow) + "." + String(p) + ".Stop");
      }
    }
    SaveSettings();
    request->redirect("/homepage");                       // Go back to home page
  });
  // Set handler for '/handlesetup' inputs
  server.on("/handlesetup", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasArg("hysteresis")) {
      String numArg = request->arg("hysteresis");
      Hysteresis = numArg.toFloat();
    }
    if (request->hasArg("frosttemp")) {
      String numArg = request->arg("frosttemp");
      FrostTemp     = numArg.toFloat();
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
    SaveSettings();
    request->redirect("/homepage");                       // Go back to home page
  });
  server.begin();
  StartSensor();
  ReadSensor();                                           // Get current sensor values
  for (int r = 0; r < SensorReadings; r++) {
    sensordata[1][r].Temp = Temperature;
    sensordata[1][r].Humi = Humidity;
  }
  ActuateHeating(OFF);                                    // Switch heating OFF
  ReadSensor();                                           // Get current sensor values
  LastTimerSwitchCheck = millis() + TimerCheckDuration;   // preload timer value with update duration
}
//#########################################################################################
void loop() {
  if (millis() > LastTimerSwitchCheck) {
    LastTimerSwitchCheck = millis() + TimerCheckDuration;          // Check at time-out for a change
    ReadSensor();                                                  // Get sensor readings, or simulated values if 'simulated' is ON
    UpdateLocalTime();                                             // Updates Time UnixTime to 'now'
    CheckTimerEvent();                                             // Check for schedules actuated
  }
  if (millis() > LastReadingCheck) {
    LastReadingCheck = millis() + LastReadingDuration * 60 * 1000; // Update reading record every ~n-mins e.g. 60,000uS = 1-min
    AssignSensorReadingsToArray();
  }
}
//#########################################################################################
void Homepage() {
  ReadSensor();
  append_HTML_header(Refresh);
  webpage += "<h2>Smart Thermostat Status</h2><br>";
  webpage += "<div class='numberCircle'><span class=" + String((RelayState == "ON" ? "'on'>" : "'off'>")) + String(Temperature, 1) + "&deg;</span></div><br><br><br>";
  webpage += "<table class='centre'>";
  webpage += "<tr>";
  webpage += "<td>Temperature</td>";
  webpage += "<td>Humidity</td>";
  webpage += "<td>Target Temperature</td>";
  webpage += "<td>Thermostat Status</td>";
  webpage += "<td>Schedule Status</td>";
  if (ManualOverride) {
    webpage += "<td>ManualOverride</td>";
  }
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td class='large'>" + String(Temperature, 1)       + "&deg;</td>";
  webpage += "<td class='large'>" + String(Humidity, 0)          + "%</td>";
  webpage += "<td class='large'>" + String(TargetTemp, 1) + "&deg;</td>";
  webpage += "<td class='large'><span class=" + String((RelayState == "ON" ? "'on'>" : "'off'>")) + RelayState + "</span></td>"; // (condition ? that : this) if this then that else this
  webpage += "<td class='large'><span class=" + String((TimerState == "ON" ? "'on'>" : "'off'>")) + TimerState + "</span></td>";
  if (ManualOverride) {
    webpage += "<td class='large'>" + String(ManualOverride ? "ON" : "OFF") + "</td>";
  }
  webpage += "</tr>";
  webpage += "</table>";
  webpage += "<br>";
  append_HTML_footer();
}
//#########################################################################################
void Graphs() {
  append_HTML_header(Refresh);
  webpage += "<h2>Thermostat Readings</h2>";
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
  webpage += "  <td><div id='chart_divTS1' style='width:50%'></div></td>";
  webpage += "  <td><div id='chart_divHS1' style='width:50%'></div></td>";
  webpage += "</tr>";
  webpage += "</table>";
  webpage += "<br>";
  webpage += "</div>";
  webpage += "<p>Heating status : <span class=" + String((RelayState == "ON" ? "'on'>" : "'off'>")) + RelayState + "</span></p>";
  append_HTML_footer();
}
//#########################################################################################
void TimerSet() {
  append_HTML_header(noRefresh);
  webpage += "<h2>Thermostat Schedule Setup</h2><br>";
  webpage += "<h3>Enter required temperatures and time, use Clock symbol for ease of time entry</h3><br>";
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
    webpage += "<tr><td>Temp</td>";
    webpage += "<td><input type='text' name='" + String(0) + "." + String(p) + ".Temp' value='"  + Timer[0].Temp[p]   + "' maxlength='5' size='6'></td>";
    for (int dow = 1; dow < 6; dow++) {
      webpage += "<td><input type='text' name='" + String(dow) + "." + String(p) + ".Temp' value='"      + Timer[dow].Temp[p] + "' maxlength='5' size='5'></td>";
    }
    webpage += "<td><input type='text' name='" + String(6) + "." + String(p) + ".Temp' value='"  + Timer[6].Temp[p]   + "' maxlength='5' size='5'></td>";
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
      for (int dow = 2; dow < 7; dow++) {
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
  webpage += "<h2>Thermostat System Setup</h2><br>";
  webpage += "<h3>Enter required parameter values</h3><br>";
  webpage += "<FORM action='/handlesetup'>";
  webpage += "<table class='centre'>";
  webpage += "<tr>";
  webpage += "<td>Setting</td><td>Value</td>";
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='hysteresis'>Hysteresis value (e.g. 0 - 1.0&deg;) [N.N]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9][.][0-9]' name='hysteresis' value='" + String(Hysteresis, 1) + "'></td>"; // 0.0 valid input style
  webpage += "</tr>";
  webpage += "<tr>";
  webpage += "<td><label for='frosttemp'>Frost Protection Temperature&deg; [NN]</label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='frosttemp' value='" + String(FrostTemp) + "'></td>"; // 00-99 valid input style
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
  webpage += "<td><label for='manualoverridetemp'>Manual Override Temperature&deg; </label></td>";
  webpage += "<td><input type='text' size='4' pattern='[0-9]*' name='manualoverridetemp' value='" + String(ManOverrideTemp, 0) + "'></td>"; // 00-99 valid input style
  webpage += "</tr>";  webpage += "</table>";
  webpage += "<br><input type='submit' value='Enter'><br><br>";
  webpage += "</form>";
  append_HTML_footer();
}
//#########################################################################################
void Help() {
  append_HTML_header(noRefresh);
  webpage += "<h2>Help</h2><br>";
  webpage += "<div style='text-align: left;font-size:1.1em;'>";
  webpage += "<br><u><b>Setup Menu</b></u>";
  webpage += "<p><i>Hysteresis</i> - this setting is used to prevent unwanted rapid switching on/off of the heating as the room temperature";
  webpage += " nears or falls towards the set/target-point temperature. A normal setting is 0.5&deg;C, the exact value depends on the environmental characteristics, ";
  webpage += "for example, where the thermostat is located and how fast a room heats or cools.</p>";
  webpage += "<p><i>Frost Protection Temperature</i> - this setting is used to protect from low temperatures and pipe freezing in cold conditions. ";
  webpage += "It helps prevent low temperature damage by turning on the heating until the risk of freezing has been prevented.</p>";
  webpage += "<p><i>Early Start Duration</i> - if greater than 0, begins heating earlier than scheduled so that the scheduled temperature is reached by the set time.</p>";
  webpage += "<p><i>Heating Manual Override</i> - switch the heating on and control to the desired temperature, switched-off when the next timed period begins.</p>";
  webpage += "<p><i>Heating Manual Override Temperature</i> - used to set the desired manual override temperature.</p>";
  webpage += "<u><b>Schedule Menu</b></u>";
  webpage += "<p>Determines the heating temperature for each day of the week and up to 4 heating periods in a day. ";
  webpage += "To set the heating to come on at 06:00 and off at 09:00 with a temperature of 20&deg; enter 20 then the required start/end times. ";
  webpage += "Repeat for each day of the week and heating period within the day for the required heat profile.</p>";
  webpage += "<u><b>Graph Menu</b></u>";
  webpage += "<p>Displays the target temperature set and the current measured temperature and humidity. ";
  webpage += "Thermostat status is also displayed as temperature varies.</p>";
  webpage += "<u><b>Status Menu</b></u>";
  webpage += "<p>Displays the current temperature and humidity. ";
  webpage += "Displays the temperature the thermostat is controlling towards, the current state of the thermostat (ON/OFF) and ";
  webpage += "timer status (ON/OFF).</p>";
  webpage += "</div>";
  append_HTML_footer();
}
//#########################################################################################
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
void CheckTimerEvent() {
  String TimeNow;
  UpdateTargetTemperature();
  TimeNow    = ConvertUnixTime(UnixTime);                  // Get the current time e.g. 15:35
  TimerState = "OFF";                                      // Switch timer off until decided by the schedule
  if (EarlyStart > 0) {                                    // If early start is enabled by a value > 0
    TimeNow = ConvertUnixTime(UnixTime + EarlyStart * 60); // Advance the clock by the Early Start Duration
  }
  if (ManualOverride == ON) {                              // If manual override is enabled then turn the heating on
    TargetTemp = ManOverrideTemp;                          // Set the target temperature to the manual overide temperature
    ControlHeating();                                      // Control the heating as normal
  }
  for (byte dow = 0; dow < 7; dow++) {                     // Look for any valid timer events, if found turn the heating on
    for (byte p = 0; p < NumOfEvents; p++) {
      // Now check for a scheduled ON time, if so Switch the Timer ON and check the temperature against target temperature
      if (String(dow) == DoW_str && (TimeNow >= Timer[dow].Start[p] && TimeNow <= Timer[dow].Stop[p] && Timer[dow].Start[p] != ""))
      {
        TimerState = "ON";
        ControlHeating();
        ManualOverride = OFF; // If it was ON turn it OFF when the timer starts a controlled period
      }
    }
  }
  CheckAndSetFrostTemperature();
}
//#########################################################################################
void ControlHeating() {
  if (Temperature < (TargetTemp - Hysteresis)) {           // Check if room temeperature is below set-point and hysteresis offset
    ActuateHeating(ON);                                    // Switch Relay/Heating ON if so
  }
  if (Temperature > (TargetTemp + Hysteresis)) {           // Check if room temeperature is above set-point and hysteresis offset
    ActuateHeating(OFF);                                   // Switch Relay/Heating OFF if so
  }
  if (Temperature > MaxTemperature) {                      // Check for faults/over-temperature
    ActuateHeating(OFF);                                   // Switch Relay/Heating OFF if temperature is above maximum temperature
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
      }
    }
  }
  if (ManualOverride == ON) TargetTemp = ManOverrideTemp;
  Serial.println("Target Temperature = " + String(TargetTemp, 1) + "°");
}
//#########################################################################################
void CheckAndSetFrostTemperature() {
  if (TimerState == "OFF" && ManualOverride == OFF) { // Only check for frost protection when heating is off
    if (Temperature < (FrostTemp - Hysteresis)) {     // Check if temperature is below Frost Protection temperature and hysteresis offset
      ActuateHeating(ON);                             // Switch Relay/Heating ON if so
      Serial.println("Frost protection actuated...");
    }
    if (Temperature > (FrostTemp + Hysteresis)) {     // Check if temerature is above Frost Protection temperature and hysteresis offset
      ActuateHeating(OFF);                            // Switch Relay/Heating OFF if so
    }
  }
}
//#########################################################################################
void ActuateHeating(bool demand) {
  pinMode(RelayPIN, OUTPUT);
  pinMode(LEDPIN, OUTPUT);
  if (demand) {
    RelayState = "ON";
    if (RelayReverse) {
      digitalWrite(RelayPIN, LOW);
    }
    else
    {
      digitalWrite(RelayPIN, HIGH);
    }
    digitalWrite(LEDPIN, LOW);
    Serial.println("Thermostat ON");
  }
  else
  {
    RelayState = "OFF";
    if (RelayReverse) {
      digitalWrite(RelayPIN, HIGH);
    }
    else
    {
      digitalWrite(RelayPIN, LOW);
    }
    digitalWrite(LEDPIN, HIGH);
    Serial.println("Thermostat OFF");
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
    Temperature = sensor.readTemperature();      // Read the current temperature
    if (Temperature >= 50 || Temperature < -30) Temperature = LastTemperature; // Check and correct any errorneous readings
    LastTemperature = Temperature;
    while (isnan(sensor.readHumidity())) { }     // Make sure there are no reading errors
    Humidity = sensor.readHumidity();
    Serial.println("Temperature = " + String(Temperature, 1) + ", Humidity = " + String(Humidity, 0));
  }
}
//#########################################################################################
void AssignSensorReadingsToArray() {
  SensorReading[1][0] = 1;
  SensorReading[1][1] = Temperature;
  SensorReading[1][2] = Humidity;
  SensorReading[1][3] = RelayState;
  AddReadingToSensorData(1, Temperature, Humidity); // Only sensor-1 is implemented here, could  be more though
}
//#########################################################################################
void AddReadingToSensorData(byte RxdFromID, float Temperature, byte Humidity) {
  byte ptr, p;
  ptr = SensorReadingPointer[RxdFromID];
  sensordata[RxdFromID][ptr].Temp = Temperature;
  sensordata[RxdFromID][ptr].Humi = Humidity;
  ptr++;
  if (ptr >= SensorReadings) {
    p = 0;
    do {
      sensordata[RxdFromID][p].Temp  = sensordata[RxdFromID][p + 1].Temp;
      sensordata[RxdFromID][p].Humi  = sensordata[RxdFromID][p + 1].Humi;
      p++;
    } while (p < SensorReadings);
    ptr = SensorReadings - 1;
    sensordata[RxdFromID][SensorReadings - 1].Temp = Temperature;
    sensordata[RxdFromID][SensorReadings - 1].Humi = Humidity;
  }
  SensorReadingPointer[RxdFromID] = ptr;
}
//#########################################################################################
void append_HTML_header(bool refreshMode) {
  webpage  = "<!DOCTYPE html><html lang='en'>";
  webpage += "<head>";
  webpage += "<title>" + sitetitle + "</title>";
  webpage += "<meta charset='UTF-8'>";
  if (refreshMode) webpage += "<meta http-equiv='refresh' content='5'>"; // 5-secs refresh time, test needed to prevent auto updates repeating some commands
  webpage += "<script src=\"https://code.jquery.com/jquery-3.2.1.min.js\"></script>";
  webpage += "<style>";
  webpage += "body             {width:68em;margin-left:auto;margin-right:auto;font-family:Arial,Helvetica,sans-serif;font-size:14px;color:blue;background-color:#e1e1ff;text-align:center;}";
  webpage += ".centre          {margin-left:auto;margin-right:auto;}";
  webpage += "h2               {margin-top:0.3em;margin-bottom:0.3em;font-size:1.4em;}";
  webpage += "h3               {margin-top:0.3em;margin-bottom:0.3em;font-size:1.2em;}";
  webpage += "h4               {margin-top:0.3em;margin-bottom:0.3em;font-size:0.8em;}";
  webpage += ".on              {color: red;}";
  webpage += ".off             {color: limegreen;}";
  webpage += ".topnav          {overflow: hidden;background-color:lightcyan;}";
  webpage += ".topnav a        {float:left;color:blue;text-align:center;padding:1em 1.14em;text-decoration:none;font-size:1.3em;}";
  webpage += ".topnav a:hover  {background-color:deepskyblue;color:white;}";
  webpage += ".topnav a.active {background-color:lightblue;color:blue;}";
  webpage += "table tr, td     {padding:0.2em 0.5em 0.2em 0.5em;font-size:1.0em;font-family:Arial,Helvetica,sans-serif;}";
  webpage += "col:first-child  {background:lightcyan}col:nth-child(2){background:#CCC}col:nth-child(8){background:#CCC}";
  webpage += "tr:first-child   {background:lightcyan}";
  webpage += ".large           {font-size:1.8em;padding:0;margin:0}";
  webpage += ".medium          {font-size:1.4em;padding:0;margin:0}";
  webpage += ".ps              {font-size:0.7em;padding:0;margin:0}";
  webpage += "#outer           {width:100%;display:flex;justify-content:center;}";
  webpage += "footer           {padding:0.08em;background-color:cyan;font-size:1.1em;}";
  webpage += ".numberCircle    {border-radius:50%;width:2.7em;height:2.7em;border:0.11em solid blue;padding:0.2em;color:blue;text-align:center;font-size:3em;";
  webpage += "                  display:inline-flex;justify-content:center;align-items:center;}";
  webpage += ".wifi            {padding:3px;position:relative;top:1em;left:0.36em;}";
  webpage += ".wifi, .wifi:before {display:inline-block;border:9px double transparent;border-top-color:currentColor;border-radius:50%;}";
  webpage += ".wifi:before     {content:'';width:0;height:0;}";
  webpage += "</style></head>";
  webpage += "<body>";
  webpage += "<div class='topnav'>";
  webpage += "<a href='/'>Status</a>";
  webpage += "<a href='graphs'>Graph</a>";
  webpage += "<a href='timer'>Schedule</a>";
  webpage += "<a href='setup'>Setup</a>";
  webpage += "<a href='help'>Help</a>";
  webpage += "<a href=''></a>";
  webpage += "<a href=''></a>";
  webpage += "<a href=''></a>";
  webpage += "<a href=''></a>";
  webpage += "<div class='wifi'/></div><span>" + WiFiSignal() + "</span>";
  webpage += "</div><br>";
}
//#########################################################################################
void append_HTML_footer() {
  webpage += "<footer>";
  webpage += "<p class='medium'>ESP Smart Thermostat</p>";
  webpage += "<p class='ps'><i>Copyright &copy;&nbsp;D L Bird " + String(Year) + " V" + version + "</i></p>";
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
  Timer[0].DoW = "Sun"; Timer[1].DoW = "Mon"; Timer[2].DoW = "Tue"; Timer[3].DoW = "Wed"; Timer[4].DoW = "Thu"; Timer[5].DoW = "Fri"; Timer[6].DoW = "Sat";
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
        dataFile.println(Timer[dow].Start[p]);
        dataFile.println(Timer[dow].Stop[p]);
        Serial.println("Period: " + String(p) + " " + Timer[dow].Temp[p] + " frpm: " + Timer[dow].Start[p] + " to: " + Timer[dow].Stop[p]);
      }
    }
    dataFile.println(Hysteresis, 1);
    dataFile.println(FrostTemp, 1);
    dataFile.println(EarlyStart);
    Serial.println("Saved Hysteresis : " + String(Hysteresis));
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
          Timer[dow].Start[p] = dataFile.readStringUntil('\n'); Timer[dow].Start[p].trim();
          Timer[dow].Stop[p]  = dataFile.readStringUntil('\n'); Timer[dow].Stop[p].trim();
          Serial.println("Period: " + String(p) + " " + Timer[dow].Temp[p] + " frpm: " + Timer[dow].Start[p] + " to: " + Timer[dow].Stop[p]);
        }
      }
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); Hysteresis = Entry.toFloat();
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); FrostTemp  = Entry.toInt();
      Entry = dataFile.readStringUntil('\n'); Entry.trim(); EarlyStart = Entry.toInt();
      Serial.println("Recovered Hysteresis : " + String(Hysteresis));
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

/*
   Version 1.0 795 lines of code
   All HTML fully validated by W3C
*/
