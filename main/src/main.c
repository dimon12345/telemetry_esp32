#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/idf_additions.h>
#include <nvs_flash.h>

#include "mqtt/mqtt.h"
#include "sensor/ds18b20_sensor.h"
#include "wifi/wifi.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Start");

    sensor_handle_t sensor_handle;
    sensor_init(&sensor_handle);

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

    esp_netif_create_default_wifi_sta();

    wifi_handle_t wifi_handle = {false, 0, NULL};
    ESP_ERROR_CHECK(wifi_connect(&wifi_handle));

    mqtt_handle_t mqtt_handle = {NULL};
    ESP_ERROR_CHECK(mqtt_connect(&mqtt_handle));

    ESP_LOGI(TAG, "All ready!");

    float temperature = 0.f;

    for (;;) {
        err = sensor_read_temperature(&sensor_handle, &temperature);
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
