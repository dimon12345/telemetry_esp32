#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "components/ds18b20_sensor.h"
#include "components/mqtt.h"
#include "components/wifi.h"
#include "components/watchdog.h"
#include "event_group_bits.h"

static const char *TAG = "MAIN";

static const uint32_t WAIT_CONNECT_TIME_MS = 60U * 1000U;
static const TickType_t WAIT_CONNECT_TIME_TICKS = pdMS_TO_TICKS(60U * 1000U);
static const uint32_t SENSOR_INTERVAL_MS =
        1000U * CONFIG_TELEMETRY_SENSOR_MESSAGE_TIME_INTERVAL;
static const uint32_t WDT_TIMEOUT_MS =
        2U * (WAIT_CONNECT_TIME_MS + SENSOR_INTERVAL_MS);
static const uint32_t RESTART_ON_ERROR_TIME_TICKS =
        pdMS_TO_TICKS(30 * 60 * 1000);

void app_main(void) {
    ESP_LOGI(TAG, "Start");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Sensor
    sensor_handle_t sensor_handle = {0};
    err = sensor_init(&sensor_handle);
    if (err != ESP_OK) {
        ESP_LOGE(
                TAG, "Sensor init error: %s (0x%X)", esp_err_to_name(err), err);
    }

    // Event group
    EventGroupHandle_t event_group = xEventGroupCreate();
    if (event_group == NULL) {
        ESP_LOGE(TAG, "No memory. Restart ...");
        esp_restart();
    }

    // Wi-Fi
    esp_netif_create_default_wifi_sta();

    wifi_handle_t wifi_handle = {.event_group = event_group};
    ESP_ERROR_CHECK(wifi_init(&wifi_handle));

    // MQTT
    mqtt_handle_t mqtt_handle = {.event_group = event_group,
            .restart_on_error_ticks = RESTART_ON_ERROR_TIME_TICKS};
    ESP_ERROR_CHECK(mqtt_init(&mqtt_handle));

    // Watchdog timer
    watchdog_init(WDT_TIMEOUT_MS);

    ESP_LOGI(TAG, "All ready!");

    TickType_t no_error_time = xTaskGetTickCount();
    float temperature = 0.f;
    err = ESP_OK;

    for (;;) {
        watchdog_feed();
        EventBits_t bits = xEventGroupWaitBits(event_group, CONNECTED_BITS,
                pdFALSE, pdTRUE, WAIT_CONNECT_TIME_TICKS);
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGE(TAG, "No Wi-Fi connection ...");
            err = ESP_ERR_INVALID_STATE;
        } else if ((bits & MQTT_CONNECTED_BIT) == 0) {
            ESP_LOGE(TAG, "No MQTT connection ...");
            err = ESP_ERR_INVALID_STATE;
        } else {
            err = sensor_read_temperature(&sensor_handle, &temperature);
            if (err == ESP_OK) {
                err = mqtt_send_temperature(&mqtt_handle, temperature);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Temperature sent: %.2f", temperature);
                } else {
                    ESP_LOGE(TAG, "MQTT send temperature error: %s (0x%X)",
                            esp_err_to_name(err), err);
                }
            } else {
                ESP_LOGE(TAG, "Read sensor error: %s (0x%X)",
                        esp_err_to_name(err), err);
            }
        }

        if (err == ESP_OK) {
            no_error_time = xTaskGetTickCount();
        } else if ((xTaskGetTickCount() - no_error_time) >=
                RESTART_ON_ERROR_TIME_TICKS) {
            ESP_LOGE(TAG, "Can't recover from errors. Restart ...");
            esp_restart();
        }

        for (int i = 0; i < CONFIG_TELEMETRY_SENSOR_MESSAGE_TIME_INTERVAL; ++i) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
