#pragma once

#include <ds18b20.h>
#include <esp_err.h>
#include <onewire_bus.h>

typedef struct {
    onewire_bus_handle_t bus;
    ds18b20_device_handle_t device;
} sensor_handle_t;

void init_sensor(sensor_handle_t *handle);
esp_err_t read_temperature(sensor_handle_t *handle, float *temperature);
