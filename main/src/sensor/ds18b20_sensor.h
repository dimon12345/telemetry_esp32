#pragma once

#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <esp_err.h>
#include <onewire_bus.h>

#include <ds18b20.h>

typedef struct {
    onewire_bus_handle_t bus;
    ds18b20_device_handle_t device;
    atomic_bool sensor_ready;
} sensor_handle_t;

esp_err_t sensor_init(sensor_handle_t *handle);
esp_err_t sensor_read_temperature(sensor_handle_t *handle, float *temperature);
