#pragma once

#define WIFI_CONNECTED_BIT (1 << 0)
#define MQTT_CONNECTED_BIT (1 << 1)
#define MQTT_SEND_BIT (1 << 2)
#define TEMPERATURE_READY_BIT (1 << 3)
#define TEMPERATURE_SEND_PENDING (1 << 4)
#define RESTART_TIMEOUT_BIT (1 << 5)
#define CONNECTED_BITS (WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT)

extern EventGroupHandle_t event_group_g;
