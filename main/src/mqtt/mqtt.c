#include "mqtt.h"

#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "sdkconfig.h"

static esp_mqtt_client_handle_t global_client = NULL;
static bool is_mqtt_connected = false;
static const char *TAG = "MQTT_PUB";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            global_client = event->client;
            is_mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            is_mqtt_connected = false;
            break;
        default:
            break;
    }
    
}

esp_err_t mqtt_connect() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_ESP_MQTT_BROKER_URI,
        .credentials = {
            .username = CONFIG_ESP_MQTT_USERNAME, 
            .authentication.password = CONFIG_ESP_MQTT_PASSWORD
        }
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    return ESP_OK;
}

void mqtt_send_temperature(float temperature) {
    if (!is_mqtt_connected || global_client == NULL) {
        if (mqtt_connect() != ESP_OK) {
            ESP_LOGW(TAG, "Can't send temperature. Broker connection lost");
            return;
        }
    }

    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temperature);

    int msg_id = esp_mqtt_client_publish(global_client, CONFIG_ESP_MQTT_TOPIC, payload, 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "MQTT temperature message published with error");
    }
}
