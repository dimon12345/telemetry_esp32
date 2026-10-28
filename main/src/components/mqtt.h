#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_err.h>

#include <mqtt_client.h>

#include "util/backoff_timer.h"

typedef struct {
    esp_mqtt_client_handle_t client;
    uint64_t remaining_ticks;
    uint64_t restart_on_error_ticks;
    backoff_handle_t backoff;
    float temperature;
} mqtt_handle_t;

esp_err_t mqtt_init(mqtt_handle_t *handle);
void mqtt_update(mqtt_handle_t *handle, TickType_t elapsed);
void mqtt_temperature_for_send(mqtt_handle_t *handle, float temperature);
