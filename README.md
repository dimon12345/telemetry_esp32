Telemetry on ESP32
==================

ESP32 + DS18B20 temperature sensor with MQTT over WiFi

Note: For the DS18B20 to function properly, a 4.7kOhm pull-up resistor
must be connected to the yellow wire of the data line. GPIO pin 22 used
by default. Pin number must be available on the board and not used by
flash/PSRAM.

No SSL, no Websockets. MQTT and Wi-Fi credentials required.
Use idf.py menuconfig.


![esp32 screenshot](https://github.com/dimon12345/telemetry_media/blob/master/screenshots/esp32.jpg)
