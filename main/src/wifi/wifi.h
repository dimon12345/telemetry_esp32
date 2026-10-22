#pragma once

#include <stdatomic.h>

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_timer.h>


typedef struct {
    _Atomic bool wifi_connected;
    _Atomic int wifi_disconnect_count;
    esp_timer_handle_t connect_timer;
} wifi_handle_t;

esp_err_t wifi_connect(wifi_handle_t *handle);
