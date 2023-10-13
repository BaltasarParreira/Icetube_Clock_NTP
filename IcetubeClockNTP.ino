/**
 * Icetube Clock NTP to GPS converter using ESP8266 or ESP32
 * V2.1 by BaltasarParreira
 * Using parts/code from NMEA CRC generator by ninetreesdesign David (https://github.com/ninetreesdesign/CRC-Checksum-Function/blob/master/ds_CRC_function.ino)
 */
//#include <Arduino.h>
#include <LittleFS.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#define WEBSERVER_H
#include <ESPAsyncWebServer.h> 
#include <ESP8266mDNS.h>

#include <WebSocketsServer.h>

AsyncWebServer server(80);
DNSServer dns; 

WebSocketsServer webSocket = WebSocketsServer(81);

#define DEBUG
#include "IPGeolocation.h"
String key;

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

bool DST_MODE = true;
int TIME_OFFSET = 1;     // Default PT Time
int DST_TIME_OFFSET;
double DST;
String NTP;
String TZID;
String IPLOC;
String FLAG;
String keyVal;
int lastsecond;
uint8_t SNUM;


const char* host = "icetube-ntp-clock";

const long utcOffsetInSeconds = 0; //3600;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

#define TRIGGER_PIN 0

// wifimanager can run in a blocking mode or a non blocking mode
// Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

// Function to generate the NMEA 8-bit CRC
////////////////////////////////////////////////////
// - A message is built and placed into a buffer string
// - The checksum is computed, and formatted to two chars
// - The full string is printed to the IDE monitor
//   EX1:  $test*   crc = 16
//   EX2:  $GPRMC,023405.00,A,1827.23072,N,06958.07877,W,1.631,33.83,230613,,,A*  crc = 42

// 2017-10-19 streamlined this to be a clearer example, posted to github

const byte buff_len = 90;
char CRCbuffer[buff_len];

// create pre-defined strings to control flexible output formatting
String sp = " ";
String delim = ",";
String splat = "*";
String msg = "";

String latitude;
String longitude;
////////////////////////////////////////////////////

void checkButton(){
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if( digitalRead(TRIGGER_PIN) == LOW ){
      Serial.println(F("Button Pressed"));
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(TRIGGER_PIN) == LOW ){
        Serial.println(F("Button Held"));
        Serial.println(F("Erasing Config, restarting"));
        wm.resetSettings();
        ESP.restart();
      }
      
      // start portal w delay
      Serial.println(F("Starting config portal"));
      wm.setConfigPortalTimeout(120);
      
      if (!wm.startConfigPortal("OnDemandAP","password")) {
        Serial.println(F("failed to connect or hit timeout"));
        delay(3000);
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println(F("connected...yeey :)"));
      }
    }
  }
}

String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
  Serial.println(F("[CALLBACK] saveParamCallback fired"));
  Serial.println(F("PARAM customfieldid = ") + getParam("customfieldid"));
}

// -----------------------------------------------------------------------
byte convertToCRC(char *buff) {
  // NMEA CRC: XOR each byte with previous for all chars between '$' and '*'
  char c;
  byte i;
  byte start_with = 0;    // index of starting char in msg
  byte end_with = 0;      // index of starting char in msg
  byte crc = 0;

  for (i = 0; i < buff_len; i++) {
    c = buff[i];
    if (c == '$') {
      start_with = i;
    }
    if (c == '*') {
      end_with = i;
    }
  }
  if (end_with > start_with) {
    for (i = start_with + 1; i < end_with; i++) { // XOR every character between '$' and '*'
      crc = crc ^ buff[i] ;  // xor the next char
    }
  }
  else { // else if error, print a msg  
    Serial.println(F("CRC ERROR"));
  }
  return crc;
  
  // based on code by Elimeléc López - July-19th-2013
}

// -----------------------------------------------------------------------
void outputMsg(String msg) {
  msg.toCharArray(CRCbuffer, sizeof(CRCbuffer)); // put complete string into CRCbuffer
  byte crc = convertToCRC(CRCbuffer); // call function to compute the crc value

  Serial.print(msg);
  if (crc < 16) Serial.print("0"); // add leading 0 if needed
  Serial.println(crc, HEX);

  Serial1.print(msg);
  if (crc < 16) Serial1.print("0"); // add leading 0 if needed
  Serial1.println(crc, HEX);
}
// -----------------------------------------------------------------------

void getLocation() {
  if (key != "") {
    IPGeolocation location(key,"ABSTRACT");
    IPGeo IPG;
    location.updateStatus(&IPG);

    if (location.getResponse().indexOf("ip_address") > 0 ) {
      // Debug JSON reply
      Serial.println(location.getResponse());
      Serial.println(IPG.city);
      Serial.println(IPG.country);
      Serial.println(IPG.country_code);
      Serial.println(IPG.tz);
      Serial.println(IPG.offset);
      Serial.println(IPG.is_dst);
      Serial.println(IPG.latitude,4);
      Serial.println(IPG.longitude,4);
      Serial.println(IPG.ip_address);
      Serial.println(IPG.isp_name);
      Serial.println(IPG.flag_png);
      ////////////////////////////////////////////////////

      DST = IPG.offset;
      TZID = IPG.country;
      TZID += "/" + IPG.city;
      TZID += "<br>" + IPG.tz;
      TZID += "<br>" + IPG.abbreviation;
      if (IPG.is_dst)
        TZID += "<br>DST is On";
      else
        TZID += "<br>DST is Off";

      IPLOC = "Country: " + IPG.country;
      IPLOC += "<br>Public IP: " + IPG.ip_address;
      IPLOC += "<br>ISP Name: " + IPG.isp_name;
      IPLOC += "<br>Latitude: " + String(IPG.latitude,4);
      IPLOC += "<br>Longitude: " + String(IPG.longitude,4);

      FLAG = IPG.flag_png;

      DST_TIME_OFFSET = int(DST);
      if (DST_MODE) {
        TIME_OFFSET = DST_TIME_OFFSET;
        //EepromWrite(); #### TO do later ??? ####
        //NTP.setTimeZone(TIME_OFFSET);
      }

      Serial.print(F("DST=")); Serial.println(DST);
      Serial.print(F("TZID=")); Serial.println(TZID);
      Serial.print(F("DST_TIME_OFFSET=")); Serial.println(DST_TIME_OFFSET);


      // Build Latitude and Longitude values
      ////////////////////////////////////////////////////
      String lat_tmp = String(IPG.latitude,4);
      String long_tmp = String(IPG.longitude,4);

      String lat_ns = "";
      if (lat_tmp.startsWith("-"))
        lat_ns = ",S";
      else
        lat_ns = ",N";
      lat_tmp.replace("-", "");

      String long_ew = "";
      if (long_tmp.startsWith("-"))
        long_ew = ",W";
      else
        long_ew = ",E";
      long_tmp.replace("-", "");

      int lat_ind = lat_tmp.indexOf('.');  //finds location of "."
      String lat_degrees = lat_tmp.substring(0, lat_ind);  //captures first data String
      String lat_decimaldegrees = lat_tmp.substring(lat_ind);  //captures second data String
      float l_decimaldegrees = lat_decimaldegrees.toFloat()*60;

      int long_ind = long_tmp.indexOf('.');  //finds location of "."
      String long_degrees = long_tmp.substring(0, long_ind);  //captures first data String
      String long_decimaldegrees = long_tmp.substring(long_ind);  //captures second data String
      float lo_decimaldegrees = long_decimaldegrees.toFloat()*60;

      char f_lat_degrees[3];
      char f_long_degrees[4];
      sprintf(f_lat_degrees, "%02s", lat_degrees);
      sprintf(f_long_degrees, "%03s", long_degrees);

      String f_l_decimaldegrees = String(l_decimaldegrees, 4);
      String f_lo_decimaldegrees = String(lo_decimaldegrees, 4);
      if (f_l_decimaldegrees.length() == 6)
        f_l_decimaldegrees = "0" + f_l_decimaldegrees;
      if (f_lo_decimaldegrees.length() == 6)
        f_lo_decimaldegrees = "0" + f_lo_decimaldegrees;

      latitude = f_lat_degrees + f_l_decimaldegrees + lat_ns;
      longitude = f_long_degrees + f_lo_decimaldegrees + long_ew;

      // Debug Latitude and Longitude values
      /*
      Serial.println(F(f_lat_degrees));
      Serial.println(F(f_l_decimaldegrees));
      Serial.println(F(latitude));

      Serial.println(F(f_long_degrees));
      Serial.println(F(f_lo_decimaldegrees));
      Serial.println(F(longitude));
      */
    } else {
      IPLOC = "<font color='red'>Invalid key.<br>Please check !!!</font>";
      FLAG = "";
      TZID = "";
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      {
      Serial.printf("[%u] Disconnected!\r\n", num);
      }
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        if (DST_MODE) {
          webSocket.sendTXT(num, "TMZON", strlen("TMZON"));
        } else {
               webSocket.sendTXT(num, "TMZOFF", strlen("TMZOFF"));
               }
                      
        char MSG[260];
        sprintf (MSG, "%01u,%s,%s,%s,%s",TIME_OFFSET,TZID.c_str(),IPLOC.c_str(),FLAG.c_str(),key.c_str());
        webSocket.sendTXT(num, MSG, strlen(MSG));

      }
      break;
    case WStype_TEXT:
      {

      Serial.printf("[%u] get Text: %s\r\n", num, payload);
      String text = String((char *) &payload[0]);
      
      if(text.startsWith("?")){
  
        String zVal=(text.substring(text.indexOf("z")+2,text.indexOf("tz=")-1));
        TIME_OFFSET = zVal.toInt();
        //Serial.println(F(zVal));

        String tzVal=(text.substring(text.indexOf("tz=")+3,text.indexOf("k=")-1));
        //Serial.println(F(tzVal));

        if (tzVal == "true") {
          DST_MODE = true;
          TIME_OFFSET = DST_TIME_OFFSET;
        } else {
                DST_MODE = false;
                }
                
        keyVal=(text.substring(text.indexOf("k=")+2,text.length()));
        //Serial.println(F(keyVal));
        
        //NTP.setTimeZone(TIME_OFFSET);
        
        //EepromWrite();

        char MSG[260];
        sprintf (MSG, "%01u,%s,%s,%s,%s",TIME_OFFSET,TZID.c_str(),IPLOC.c_str(),FLAG.c_str(),keyVal.c_str());
        webSocket.sendTXT(num, MSG, strlen(MSG));
        SNUM = num;
        
       }
      }
      break;
    case WStype_BIN:
      {
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      }
      break;
    default:
      {
      Serial.printf("Invalid WStype [%d]\r\n", type);
      }
      break;
  }
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void writeFile(const char * path, const String message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = LittleFS.open(path, "w");
    if(!file){
        Serial.println(F("- failed to open file for writing"));
        return;
    }

    if(file.print(message)){
        Serial.println(F("- file written"));
    } else {
        Serial.println(F("- write failed"));
    }

    file.close();
}

void readFile(const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = LittleFS.open(path, "r");
    if(!file || file.isDirectory()){
        Serial.println(F("- failed to open file for reading"));
        return;
    }

    Serial.println(F("- read from file:"));
    while(file.available()){
        key = file.readStringUntil('\n');
    }

    file.close();

    // Start IP Geolocalization
    ////////////////////////////////////////////////////
    if (path == "/key.txt") {
      getLocation();
    }
}


void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  Serial.begin(115200);
  Serial1.begin(9600);
  Serial.setDebugOutput(false);  
  delay(3000);
  Serial.println(F("\n Starting"));

  pinMode(TRIGGER_PIN, INPUT);
  
  // wm.resetSettings(); // wipe settings

  if(wm_nonblocking) wm.setConfigPortalBlocking(false);

  // add a custom input field
  int customFieldLength = 40;

  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");
  
  // test custom html input type(checkbox)
  // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type
  
  // test custom html(radio)
  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input
  
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);

  // custom menu via array or vector
  // 
  // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
  // const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
  // wm.setMenu(menu,6);
  std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  //set static ip
  // wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
  // wm.setShowStaticFields(true); // force show static ip fields
  // wm.setShowDnsFields(true);    // force show dns field always

  // wm.setConnectTimeout(20); // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
  // wm.setCaptivePortalEnable(false); // disable captive portal redirection
  // wm.setAPClientCheck(true); // avoid timeout if client connected to softap

  // wifi scan settings
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  // wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons
  
  // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
   res = wm.autoConnect("IcetubeClockAP"); // anonymous ap
  //res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if(!res) {
    Serial.println(F("Failed to connect or hit timeout"));
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println(F("Connected...yeey :)"));
  }

  // Start NTP
  ////////////////////////////////////////////////////
  timeClient.begin();
  ////////////////////////////////////////////////////

  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed"));
    return;
  }

  readFile("/key.txt"); 

  //server.on("/", handleRoot);
  // send a file when / is requested
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/setup.html");
  });

  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/update.html");
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      Serial.printf("Update Start: %s\n", filename.c_str());
      Update.runAsync(true);
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
        Update.printError(Serial);
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  //server.onNotFound(handleNotFound);
  server.onNotFound(notFound);

  MDNS.begin(host);
  
  //httpUpdater.setup(&server);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

} 

void loop() {
  if(wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code  
  checkButton();

  // put your main code here, to run repeatedly:
  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();

 //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;

  /*
  // NMEA message examples 
  char gps[7][80] = {
      "$GPGGA,220112.00,2118.99192,N,15751.19869,W,2,10,0.83,38.7,M,-20.2,M,,0000*4B",
      "$GPGSA,A,3,30,28,19,42,03,06,50,02,07,09,,,1.63,0.83,1.40*03",
      "$GPGSV,4,1,15,02,33,254,30,03,11,047,11,05,08,207,17,06,56,315,23*77",
      "$GPGSV,4,2,15,07,14,158,19,09,10,124,10,13,05,238,,17,25,007,*7B",
      "$GPGSV,4,3,15,19,23,338,16,28,60,062,40,30,40,180,28,35,38,093,39*79",
      "$GPRMC,220112.00,A,2118.99202,N,15751.19867,W,0.025,,"+timeClient.getDay()+",,,D*77",
      "$GPVTG,,T,,M,0.025,N,0.046,K,D*23"
  };
  */

  String time = timeClient.getFormattedTime();
  String finaltime = time;
  finaltime.replace(":","");
  char date[9];
  sprintf(date, "%02hhu%02hhu%02hhu", monthDay, currentMonth, currentYear%100);

  // Build NMEA message
  String cmd = "$GPRMC";    // a command name
  msg = cmd + delim + finaltime + ".00,A," + latitude + delim + longitude +",0.0,038.1" + delim + date + ",,,A" + splat;
  if (timeClient.getSeconds() != lastsecond) {
    outputMsg(msg); // print the entire message string, and append the CRC
    lastsecond = timeClient.getSeconds();
  }

  //delay(1000);

  if(shouldReboot){
    Serial.println(F("Rebooting..."));
    delay(100);
    ESP.restart();
  }

  if (key != keyVal && keyVal != "") {
    key = keyVal;
    writeFile("/key.txt", keyVal);

    // Start IP Geolocalization
    ////////////////////////////////////////////////////
    getLocation();

    char MSG[260];
    sprintf (MSG, "%01u,%s,%s,%s,%s",TIME_OFFSET,TZID.c_str(),IPLOC.c_str(),FLAG.c_str(),keyVal.c_str());
    webSocket.sendTXT(SNUM, MSG, strlen(MSG));
  }

}
