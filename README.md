# Icetube_Clock_NTP
# NTP to GPS for Icetube clock using any ESP8266 or ESP32 #

With this code and using one of this boards (ESP8266 or ESP32) we can connect to the internet (NTP and geolocalization by IP) and get all the necessary data for creating the same NMEA GPS sentences (spoof or fake them) and send them through the serial port pins of the ESP connected to the GPS pins on the clock board.

Like this we have the same synchronized time as the clock will think that has a GPS board connected to it. ; )

NOTE: Added my fixed/changed version of the IPGeolocation library. Download it from here (https://github.com/BaltasarParreira/IPGeolocation) and install it on your Arduino IDE libraries folder !!!
