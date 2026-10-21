#include <stdbool.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "mqtt/mqtt.h"
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

    wifi_connect();

    mqtt_handle_t mqtt_handle;
    ESP_ERROR_CHECK(mqtt_connect(&mqtt_handle));

    sensor_handle_t sensor_handle;
    init_sensor(&sensor_handle);

    ESP_LOGI(TAG, "All ready!");

    float temperature = 0.f;

    while (true) {
        err = read_temperature(&sensor_handle, &temperature);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read sensor error");
            vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_SENSOR_ERROR_SLEEP_TIME));
            continue;
        }

        err = mqtt_send_temperature(&mqtt_handle, temperature);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature sent: %.2f", temperature);
        } else {
            ESP_LOGE(TAG, "MQTT publish error.");
            vTaskDelay(
                pdMS_TO_TICKS(1000 * CONFIG_ESP_MQTT_LOST_WIFI_SLEEP_TIME));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_SENSOR_MESSAGE_TIME_INTERVAL));
    }
}
