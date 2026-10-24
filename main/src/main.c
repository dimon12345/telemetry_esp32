#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/idf_additions.h>
#include <nvs_flash.h>

#include "event_group_bits.h"
#include "freertos/projdefs.h"
#include "mqtt/mqtt.h"
#include "portmacro.h"
#include "sensor/ds18b20_sensor.h"
#include "wifi/wifi.h"

static const char *TAG = "MAIN";

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

    xEventGroupWaitBits(
            event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    // MQTT
    mqtt_handle_t mqtt_handle = {.event_group = event_group};
    ESP_ERROR_CHECK(mqtt_init(&mqtt_handle));

    ESP_LOGI(TAG, "All ready!");

    TickType_t no_error_time = xTaskGetTickCount();
    TickType_t restart_on_errors_timeout_ticks =
            pdMS_TO_TICKS(30 * 60 * 1000); // 30 min

    float temperature = 0.f;

    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(
                event_group, CONNECTED_BITS, pdFALSE, pdTRUE, restart_on_errors_timeout_ticks);
        if ((bits & CONNECTED_BITS) != CONNECTED_BITS) {
            ESP_LOGE(TAG, "Connection cannot be established. Restart ...");
            esp_restart();
        }

        err = sensor_read_temperature(&sensor_handle, &temperature);
        if (err == ESP_OK) {
            err = mqtt_send_temperature(&mqtt_handle, temperature);
            if (err == ESP_OK) {
                no_error_time = xTaskGetTickCount();
                ESP_LOGI(TAG, "Temperature sent: %.2f", temperature);
            } else {
                ESP_LOGE(TAG, "MQTT send temperature error: %s (0x%X)",
                        esp_err_to_name(err), err);
            }
        } else {
            ESP_LOGE(TAG, "Read sensor error: %s (0x%X)", esp_err_to_name(err),
                    err);
        }

        if (err != ESP_OK) {
            if ((xTaskGetTickCount() - no_error_time) >=
                    restart_on_errors_timeout_ticks) {
                ESP_LOGE(TAG, "Can't recovery from errors. Restart ...");
                esp_restart();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(
                1000 * CONFIG_TELEMETRY_SENSOR_MESSAGE_TIME_INTERVAL));
    }
}
