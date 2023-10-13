# Icetube_Clock_NTP
# NTP to GPS for Icetube clock using any ESP8266 or ESP32 #

-----------------------------------------------------------------------------------------
2023/10/12 - UPDATE TO VERSION 2.1
- abstractapi.com key can now be written on setup webpage so it's stored on ESP FS.
- Some commented old code cleanup.
- Some other improvements.
-----------------------------------------------------------------------------------------
2023/10/10 - UPDATE TO VERSION 2.0
- Now with webpage showing all IP Geolocation information and also preparing for soon having auto Region/DST for Icetube Clock so no need to adjust Region and DST on the clock menu itself.
- OTA Firmware webpage for uploading new \<file\>.bin versions easier without the need to connect USB cable on "http://\<ESP IP\>/update".
- IP Geolocation information and future auto Region/DST on/off on "http://\<ESP IP\>/setup".

NOTES: 
- Upload the contents of "data" folder with LittleFS to the ESP first !!!
- It needs my version of IPGeolocation library so it gives the new information returned by abstractapi.com on ESP webpage !!!
- Auto Region/DST is not function yet as the Icetube Clock firmware needs changes to accept this !!!
-----------------------------------------------------------------------------------------

With this code and using one of this boards (ESP8266 or ESP32) we can connect to the internet (NTP and geolocalization by IP) and get all the necessary data for creating the same NMEA GPS sentences (spoof or fake them) and send them through the serial port pins of the ESP connected to the GPS pins on the clock board.

Like this we have the same synchronized time as the clock will think that has a GPS board connected to it. ; )

NOTE: Added my fixed/changed version of the IPGeolocation library. Download it from here (https://github.com/BaltasarParreira/IPGeolocation) and install it on your Arduino IDE libraries folder !!!


WIRING:

Icetube Clock --------> ESP8266 or ESP32
------------------------------------------------------------------
GND  ------------------------------->  GND

RO (5V)  ---------------------------->  VIN

RXD  ------------------------------->  TXD

NOTE: If you use a ESP8266 model ESP-01 since as only one serial output you have to comment out from the code all Serial.print(ln) commands for information and debugging and change all "Serial1." --> to --> "Serial." !!!
On this boards TXD pin will be as on this image the 2 --> GPIO1 --> UOTXD . Also you will need some sort of voltage regulator to drop the 5V from the clock to the 3.3V needed by this ESP or you can burn it.

![image](https://github.com/johngarchie/xmas-icetube/assets/1650425/21afa672-2b4b-474e-a669-da5f2fe85820)


All other ESP8266 boards have a second serial port but only with TXD and no RXD, but since is not required no problem with this. The pin usually is marked as D4 --> GPIO2 --> TXD1.
