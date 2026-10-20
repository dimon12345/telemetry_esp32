#include "mqtt.h"

#include <stdio.h>

#include <esp_log.h>
#include <sdkconfig.h>

#include "err.h"

static const char *TAG = "MQTT_PUB";

void mqtt_init(mqtt_handle_t *handle) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_ESP_MQTT_BROKER_URI,
        .credentials = {
            .username = CONFIG_ESP_MQTT_USERNAME, 
            .authentication.password = CONFIG_ESP_MQTT_PASSWORD
        }
    };

    handle->client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(handle->client);
}

esp_err_t mqtt_send_temperature(mqtt_handle_t *handle, float temperature) {
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temperature);

    int msg_id = esp_mqtt_client_publish(handle->client, CONFIG_ESP_MQTT_TOPIC,
            payload, 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGI(TAG, "MQTT publish message error.");
        return ESP_ERR_MQTT_PUBLISH;
    }
    ESP_LOGI(TAG,"MQTT message id: %d", msg_id);
    return ESP_OK;
}
