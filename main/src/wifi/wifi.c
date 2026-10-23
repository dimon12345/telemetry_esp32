#include "wifi.h"
#include "esp_system.h"

#include <stdatomic.h>
#include <stddef.h>

#include <esp_log.h>
#include <freertos/idf_additions.h>

#include "event_group_bits.h"

#define ATOMIC_LOAD_TIME_INTERVAL 100
static const char *TAG = "WIFI";

static void timer_wifi_connect(void *arg) { esp_wifi_connect(); }

static void request_wifi_connect(wifi_handle_t *handle, int sleep_seconds) {
    uint64_t delay_us = (uint64_t)sleep_seconds * 1000 * 1000;
    if (esp_timer_is_active(handle->connect_timer)) {
        return;
    }

    ESP_ERROR_CHECK(esp_timer_start_once(handle->connect_timer, delay_us));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
        int32_t event_id, void *event_data) {
    wifi_handle_t *handle = (wifi_handle_t *)arg;
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(handle->event_group, WIFI_CONNECTED_BIT);
        atomic_fetch_add(&handle->wifi_disconnect_count, 1);
        if (atomic_load(&handle->wifi_disconnect_count) >=
                CONFIG_ESP_WIFI_DISCONNECT_THRESHOLD) {
            ESP_LOGE(TAG, "Wi-Fi disconnect threshold exceeded. Restart ...");
            esp_restart();
        }

        wifi_event_sta_disconnected_t *event =
                (wifi_event_sta_disconnected_t *)event_data;
        switch (event->reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            ESP_LOGE(TAG, "Wi-Fi password error");
            request_wifi_connect(
                    handle, CONFIG_ESP_WIFI_BAD_PASSWORD_SLEEP_TIME);
            break;

        case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGE(TAG, "Wi-Fi Access Point not found %d",
                    handle->wifi_disconnect_count);
            request_wifi_connect(handle, CONFIG_ESP_WIFI_AP_SCAN_SLEEP_TIME);
            break;

        default:
            ESP_LOGE(TAG, "Wi-Fi disconnected with reason %d", event->reason);
            request_wifi_connect(handle, CONFIG_ESP_WIFI_RECONNECT_SLEEP_TIME);
            break;
        }
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
        int32_t event_id, void *event_data) {
    wifi_handle_t *handle = (wifi_handle_t *)arg;
    xEventGroupSetBits(handle->event_group, WIFI_CONNECTED_BIT);
    atomic_store(&handle->wifi_disconnect_count, 0);
}

void init_wifi_timer(wifi_handle_t *handle) {
    const esp_timer_create_args_t once_timer_args = {
            .callback = &timer_wifi_connect, .arg = handle};

    ESP_ERROR_CHECK(esp_timer_create(&once_timer_args, &handle->connect_timer));
}

esp_err_t wifi_init(wifi_handle_t *handle) {
    init_wifi_timer(handle);

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG,
            "Wi-Fi init error");

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, handle, NULL),
            TAG, "Wi-Fi event register error");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT,
            IP_EVENT_STA_GOT_IP, &got_ip_event_handler, handle, NULL), TAG,
            "Got IP event register error");

    wifi_config_t wifi_config = {
            .sta = {.ssid = CONFIG_ESP_WIFI_SSID,
                    .password = CONFIG_ESP_WIFI_PASSWORD,
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK},
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG,
            "STA set mode error");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG,
            "Wi-Fi set config error");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start error");

    ESP_LOGI(TAG, "Connecting Wi-Fi ...");
    return ESP_OK;
}
