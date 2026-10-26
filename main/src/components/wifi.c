#include "wifi.h"

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>

#include <esp_check.h>
#include <esp_log.h>

#include "event_group_bits.h"

#define WIFI_MAX_RETRY_DELAY_S 60
#define WIFI_MIN_RETRY_DELAY_S 1

static const char *TAG = "WIFI";

_Static_assert(sizeof(CONFIG_TELEMETRY_WIFI_SSID) > 1, "Wi-Fi SSID is empty");
_Static_assert(
        sizeof(CONFIG_TELEMETRY_WIFI_PASSWORD) > 1, "Wi-Fi password is empty");

static void request_retry_connect(wifi_handle_t *handle) {
    assert(handle != NULL);

    uint64_t delay_us = (uint64_t)handle->retry_delay * 1000 * 1000;
    esp_err_t err = esp_timer_start_once(handle->connect_timer, delay_us);
    if (err == ESP_ERR_INVALID_STATE) {
        return;
    } else if (err != ESP_OK) {
        ESP_LOGE(
                TAG, "Timer start error: %s (0x%X)", esp_err_to_name(err), err);
        return;
    }

    int delay =
            atomic_load_explicit(&handle->retry_delay, memory_order_relaxed);
    if (delay < WIFI_MAX_RETRY_DELAY_S) {
        delay *= 2;
        if (delay > WIFI_MAX_RETRY_DELAY_S) {
            delay = WIFI_MAX_RETRY_DELAY_S;
        }

        atomic_store_explicit(
                &handle->retry_delay, delay, memory_order_relaxed);
    }
}

static void wifi_try_connect(wifi_handle_t *handle) {
    assert(handle != NULL);

    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connect error: %s (0x%X)", esp_err_to_name(err),
                err);
        request_retry_connect(handle);
    }
}

static void timer_wifi_connect_cb(void *arg) {
    assert(arg != NULL);

    wifi_handle_t *handle = (wifi_handle_t *)arg;
    wifi_try_connect(handle);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
        int32_t event_id, void *event_data) {
    assert(arg != NULL);

    wifi_handle_t *handle = (wifi_handle_t *)arg;
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        wifi_try_connect(handle);
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(handle->event_group, WIFI_CONNECTED_BIT);
        wifi_event_sta_disconnected_t *event =
                (wifi_event_sta_disconnected_t *)event_data;
        switch (event->reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            ESP_LOGE(TAG, "Wi-Fi password error");
            break;

        case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGE(TAG, "Wi-Fi Access Point not found");
            break;

        default:
            ESP_LOGE(TAG, "Wi-Fi disconnected with reason %d", event->reason);
            break;
        }

        request_retry_connect(handle);
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
        int32_t event_id, void *event_data) {
    assert(arg != NULL);

    wifi_handle_t *handle = (wifi_handle_t *)arg;
    xEventGroupSetBits(handle->event_group, WIFI_CONNECTED_BIT);
    atomic_store_explicit(
            &handle->retry_delay, WIFI_MIN_RETRY_DELAY_S, memory_order_relaxed);
}

esp_err_t wifi_init(wifi_handle_t *handle) {
    assert(handle != NULL);

    if (handle->connect_timer) {
        return ESP_ERR_INVALID_STATE;
    }

    atomic_store_explicit(
            &handle->retry_delay, WIFI_MIN_RETRY_DELAY_S, memory_order_relaxed);

    const esp_timer_create_args_t once_timer_args = {
            .callback = &timer_wifi_connect_cb, .arg = handle};
    ESP_RETURN_ON_ERROR(
            esp_timer_create(&once_timer_args, &handle->connect_timer), TAG,
            "Create timer error");

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "Wi-Fi init error");

    ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, handle, NULL),
            TAG, "Wi-Fi event register error");
    ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &got_ip_event_handler, handle, NULL),
            TAG, "Got IP event register error");

    wifi_config_t wifi_config = {
            .sta = {.ssid = CONFIG_TELEMETRY_WIFI_SSID,
                    .password = CONFIG_TELEMETRY_WIFI_PASSWORD,
                    .threshold.authmode = WIFI_AUTH_WPA_PSK,
                    .sae_pwe_h2e = WPA3_SAE_PWE_BOTH},
    };

    ESP_RETURN_ON_ERROR(
            esp_wifi_set_mode(WIFI_MODE_STA), TAG, "STA set mode error");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG,
            "Wi-Fi set config error");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start error");

    ESP_LOGI(TAG, "Connecting Wi-Fi ...");
    return ESP_OK;
}
