#pragma once

#include "onewire_bus.h"
#include "ds18b20.h"

typedef struct {
    onewire_bus_handle_t bus;
    ds18b20_device_handle_t device;
} sensor_handle_t;

esp_err_t init_sensor(sensor_handle_t *handle, int bus_gpio_num);
esp_err_t read_temperature(sensor_handle_t *handle, float *temperature);
