#pragma once

#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_err.h>
#include <onewire_bus.h>

#include <ds18b20.h>

#include "util/backoff_timer.h"

typedef struct {
    onewire_bus_handle_t bus;
    ds18b20_device_handle_t device;
    bool sensor_ready;
    float temperature;
    backoff_handle_t backoff;
} sensor_handle_t;

esp_err_t sensor_init(sensor_handle_t *handle);
void sensor_update(sensor_handle_t *handle, TickType_t elapsed);
esp_err_t sensor_read_temperature(sensor_handle_t *handle, float *temperature);
