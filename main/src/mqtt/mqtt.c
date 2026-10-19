#include "mqtt.h"

#include "esp_err.h"
#include "esp_log.h"

#include "sdkconfig.h"

static const char *TAG = "MQTT_PUB";

esp_err_t mqtt_connect(mqtt_handle_t *handle) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_ESP_MQTT_BROKER_URI,
        .credentials = {
            .username = CONFIG_ESP_MQTT_USERNAME, 
            .authentication.password = CONFIG_ESP_MQTT_PASSWORD
        }
    };

    handle->client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(handle->client);

    return ESP_OK;
}

void mqtt_send_temperature(mqtt_handle_t *handle, float temperature) {
    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temperature);

    int msg_id = esp_mqtt_client_publish(handle->client,
            CONFIG_ESP_MQTT_TOPIC, payload, 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "MQTT temperature message published with error");
    }
}
