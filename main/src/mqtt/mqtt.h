#pragma once

#include "mqtt_client.h"

typedef struct {
    esp_mqtt_client_handle_t client;
} mqtt_handle_t;

esp_err_t mqtt_connect(mqtt_handle_t *handle);
void mqtt_send_temperature(mqtt_handle_t *handle, float temperature);
