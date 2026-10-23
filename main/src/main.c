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

#define CHECK_EVENT_GROUP_BIT_TIME_INTERVAL 1

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

    EventGroupHandle_t event_group = xEventGroupCreate();
    if (event_group == NULL) {
        ESP_LOGE(TAG, "No memory. Restart ...");
        esp_restart();
    }

    // Wi-Fi
    esp_netif_create_default_wifi_sta();

    wifi_handle_t wifi_handle = {.event_group = event_group};
    int init_try = 0;
    for (; init_try < CONFIG_TELEMETRY_SENSOR_ERROR_INIT_THRASHOLD; ++init_try) {
        if (wifi_init(&wifi_handle) == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_TELEMETRY_WIFI_ERROR_INIT_SLEEP_TIME));
    }
    if (init_try >= CONFIG_TELEMETRY_SENSOR_ERROR_INIT_THRASHOLD) {
        ESP_LOGE(TAG, "Wi-Fi init error thrashold exceeded. Restart ...");
        esp_restart();
    }

    // Sensor
    sensor_handle_t sensor_handle = {.event_group = event_group};

    init_try = 0;
    for (; init_try < CONFIG_TELEMETRY_SENSOR_ERROR_THRASHOLD; ++init_try) {
        if (sensor_init(&sensor_handle) == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_TELEMETRY_SENSOR_ERROR_SLEEP_TIME));
    }

    if (init_try >= CONFIG_TELEMETRY_SENSOR_ERROR_THRASHOLD) {
        ESP_LOGE(TAG, "Sensor init error thrashold exceeded. Restart ...");
        esp_restart();
    }

    // MQTT
    mqtt_handle_t mqtt_handle = {.event_group = event_group};
    ESP_ERROR_CHECK(mqtt_init(&mqtt_handle));

    ESP_LOGI(TAG, "All ready!");

    float temperature = 0.f;

    int read_try = 0;
    for (;;) {
        xEventGroupWaitBits(event_group, MQTT_CONNECTED_BIT | SENSOR_READY_BIT,
            pdFALSE, pdTRUE, portMAX_DELAY);

        err = sensor_read_temperature(&sensor_handle, &temperature);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read sensor error");
            vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_TELEMETRY_SENSOR_ERROR_SLEEP_TIME));
            if (read_try++ >= CONFIG_TELEMETRY_SENSOR_ERROR_THRASHOLD) {
                ESP_LOGE(TAG, "Sensor read error thrashold exceeded. Restart ...");
                esp_restart();
            }
            continue;
        }
        read_try = 0;

        err = mqtt_send_temperature(&mqtt_handle, temperature);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature sent: %.2f", temperature);
        } else {
            ESP_LOGE(TAG, "MQTT publish error.");
            vTaskDelay(
                    pdMS_TO_TICKS(1000 * CONFIG_TELEMETRY_MQTT_LOST_WIFI_SLEEP_TIME));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_TELEMETRY_SENSOR_MESSAGE_TIME_INTERVAL));
    }
}
