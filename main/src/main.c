#include <stdbool.h>
#include <unistd.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "sensor/ds18b20_sensor.h"
#include "wifi/wifi.h"
#include "mqtt/mqtt.h"

#define SENSOR_ONEWIRE_BUS_GPIO 22

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(wifi_connect());
    while(!wifi_is_connected()) {
        ESP_LOGI(TAG, "Connecting wifi ..");
        sleep(1);
    }

    mqtt_handle_t mqtt_handle;
    ESP_ERROR_CHECK(mqtt_connect(&mqtt_handle));

    sensor_handle_t sensor_handle;
    ESP_ERROR_CHECK(init_sensor(&sensor_handle, SENSOR_ONEWIRE_BUS_GPIO));

    ESP_LOGI(TAG, "All ready!");
    float temperature = 0.f;
    while (true) {
        ESP_ERROR_CHECK(read_temperature(&sensor_handle, &temperature));
        mqtt_send_temperature(&mqtt_handle, temperature);

        ESP_LOGI(TAG, "Current temperature: %.2f", temperature);
        sleep(60);
    }
}
