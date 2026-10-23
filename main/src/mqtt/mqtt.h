#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <esp_err.h>

#include <mqtt_client.h>

typedef struct {
    esp_mqtt_client_handle_t client;
    EventGroupHandle_t event_group;
} mqtt_handle_t;

esp_err_t mqtt_init(mqtt_handle_t *handle);
esp_err_t mqtt_send_temperature(mqtt_handle_t *handle, float temperature);
