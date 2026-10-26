#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <portmacro.h>

#include "components/ds18b20_sensor.h"
#include "components/mqtt.h"
#include "components/wifi.h"
#include "event_group_bits.h"

static const char *TAG = "MAIN";

static const uint32_t WAIT_CONNECT_TIME_MS = 60U * 1000U;
static const TickType_t WAIT_CONNECT_TIME_TICKS =
        pdMS_TO_TICKS(WAIT_CONNECT_TIME_MS);
static const uint32_t MAIN_LOOP_DELAY_MS = 1000U;
static const TickType_t MAIN_LOOP_DELAY_TICKS =
        pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS);
static const uint32_t RESTART_ON_ERROR_TIME_TICKS =
        pdMS_TO_TICKS(30U * 60U * 1000U);
static const uint64_t SENSOR_INTERVAL_TICKS =
        (uint64_t)CONFIG_TELEMETRY_SENSOR_MESSAGE_TIME_INTERVAL *
        (uint64_t)pdMS_TO_TICKS(1000U);
static const uint32_t SENSOR_MAX_ERROR_BACKOFF_INTERVAL =
        pdMS_TO_TICKS(60U * 1000U);
static const uint32_t SENSOR_ERROR_INITIAL_BACKOFF_TICKS = pdMS_TO_TICKS(1000U);

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

    // Sensor
    sensor_handle_t sensor_handle = {0};

    ESP_LOGI(TAG, "All ready!");

    TickType_t first_error_time = 0;
    float temperature = 0.f;

    uint32_t last_time_ticks = 0U;

    uint64_t send_timeout_ticks = 0ULL;
    uint64_t sensor_error_backoff_ticks = 0ULL;
    uint64_t sensor_error_backoff_interval_ticks =
            SENSOR_ERROR_INITIAL_BACKOFF_TICKS;

    for (;;) {
        err = ESP_OK;

        EventBits_t bits = xEventGroupWaitBits(event_group, CONNECTED_BITS,
                pdFALSE, pdTRUE, WAIT_CONNECT_TIME_TICKS);
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGE(TAG, "No Wi-Fi connection ...");
            err = ESP_ERR_INVALID_STATE;
        } else if ((bits & MQTT_CONNECTED_BIT) == 0) {
            ESP_LOGE(TAG, "No MQTT connection ...");
            err = ESP_ERR_INVALID_STATE;
        }

        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed = now - last_time_ticks;
        last_time_ticks = now;

        if (send_timeout_ticks > 0ULL) {
            send_timeout_ticks = (uint64_t)elapsed >= send_timeout_ticks
                    ? 0ULL
                    : send_timeout_ticks - (uint64_t)elapsed;
        }

        if (sensor_error_backoff_ticks > 0ULL) {
            sensor_error_backoff_ticks =
                    (uint64_t)elapsed >= sensor_error_backoff_ticks
                    ? 0ULL
                    : sensor_error_backoff_ticks - (uint64_t)elapsed;

            if (sensor_error_backoff_ticks > 0) {
                err = ESP_ERR_INVALID_STATE;
            }
        }

        if (err == ESP_OK && send_timeout_ticks == 0 &&
                sensor_error_backoff_ticks == 0) {
            err = sensor_read_temperature(&sensor_handle, &temperature);
            if (err == ESP_OK) {
                err = mqtt_send_temperature(&mqtt_handle, temperature);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Temperature sent: %.2f", temperature);
                    send_timeout_ticks = SENSOR_INTERVAL_TICKS;
                    last_time_ticks = xTaskGetTickCount();
                    sensor_error_backoff_ticks =
                            SENSOR_ERROR_INITIAL_BACKOFF_TICKS;
                    sensor_error_backoff_interval_ticks = 0ULL;
                } else {
                    ESP_LOGE(TAG, "MQTT send temperature error: %s (0x%X)",
                            esp_err_to_name(err), err);
                }
            } else {
                ESP_LOGE(TAG, "Read sensor error: %s (0x%X)",
                        esp_err_to_name(err), err);
            }

            if (err != ESP_OK) {
                sensor_error_backoff_ticks =
                        sensor_error_backoff_interval_ticks;
                sensor_error_backoff_interval_ticks *= 2;
                if (sensor_error_backoff_interval_ticks >
                        SENSOR_MAX_ERROR_BACKOFF_INTERVAL) {
                    sensor_error_backoff_interval_ticks =
                            SENSOR_MAX_ERROR_BACKOFF_INTERVAL;
                }
            }
        }

        if (err == ESP_OK) {
            first_error_time = 0;
        } else if (first_error_time == 0) {
            first_error_time = xTaskGetTickCount();
        } else if ((xTaskGetTickCount() - first_error_time) >=
                RESTART_ON_ERROR_TIME_TICKS) {
            ESP_LOGE(TAG, "Can't recover from errors. Restart ...");
            esp_restart();
        }

        vTaskDelay(MAIN_LOOP_DELAY_TICKS);
    }
}
