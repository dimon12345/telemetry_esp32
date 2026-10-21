#include "wifi.h"

#include <stdatomic.h>
#include <stddef.h>
#include <unistd.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_timer.h>

#include "sdkconfig.h"

#define ATOMIC_LOAD_TIME_INTERVAL 100
static const char *TAG = "WIFI";
static _Atomic bool wifi_connected_s = false;
static esp_timer_handle_t connect_timer = NULL;


static void timer_wifi_connect(void* arg)
{
    ESP_LOGE(TAG, "timer_wifi_connect()");
    esp_wifi_connect();
}

static void request_wifi_connect(int sleep_seconds) {
    ESP_LOGE(TAG, "request_wifi_connect()");
    uint64_t delay_us = sleep_seconds * 1000 * 1000;
    if (esp_timer_is_active(connect_timer)) {
        return;
    }

    ESP_ERROR_CHECK(esp_timer_start_once(connect_timer, delay_us));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        atomic_store(&wifi_connected_s, false);

        wifi_event_sta_disconnected_t *event =
            (wifi_event_sta_disconnected_t *)event_data;
        switch (event->reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            ESP_LOGE(TAG, "Wi-Fi password error");
            request_wifi_connect(CONFIG_ESP_WIFI_BAD_PASSWORD_SLEEP_TIME);
            break;

        case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGE(TAG, "Wi-Fi Access Point not found");
            request_wifi_connect(CONFIG_ESP_WIFI_AP_SCAN_SLEEP_TIME);
            break;

        default:
            ESP_LOGE(TAG, "Wi-Fi disconnected with reason %d", event->reason);
            request_wifi_connect(CONFIG_ESP_WIFI_RECONNECT_SLEEP_TIME);
            break;
        }
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
    atomic_store(&wifi_connected_s, true);
}

void init_wifi_timer(void)
{
    const esp_timer_create_args_t once_timer_args = {
        .callback = &timer_wifi_connect
    };

    ESP_ERROR_CHECK(esp_timer_create(&once_timer_args, &connect_timer));
}

void wifi_connect() {
    init_wifi_timer();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = CONFIG_ESP_WIFI_SSID,
                .password = CONFIG_ESP_WIFI_PASSWORD,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting Wi-Fi ...");
    while (!atomic_load(&wifi_connected_s)) {
        vTaskDelay(pdMS_TO_TICKS(ATOMIC_LOAD_TIME_INTERVAL));
    }
}
