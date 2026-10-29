#pragma once

#include <stdatomic.h>

#include <freertos/FreeRTOS.h>

#include <esp_err.h>
#include <esp_timer.h>
#include <esp_wifi.h>

typedef struct {
    esp_timer_handle_t connect_timer;
    atomic_int retry_delay;
} wifi_handle_t;

esp_err_t wifi_init(wifi_handle_t *handle);
