#include "mqtt.h"

#include <stdatomic.h>
#include <stdio.h>

#include <esp_log.h>
#include <sdkconfig.h>

#include "err.h"

#define MQTT_QOS 1

static const char *TAG = "MQTT";
static _Atomic bool mqtt_connected_s = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_ERROR:
            if (event->error_handle->error_type ==
                    MQTT_ERROR_TYPE_CONNECTION_REFUSED) {

                esp_mqtt_connect_return_code_t return_code =
                        event->error_handle->connect_return_code;
                if (return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME ||
                    return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) {
                    ESP_LOGE(TAG, "Authorization error: Bad user or password "
                                  "(Return code: %d)", return_code);
                }
            }
            break;

        case MQTT_EVENT_CONNECTED:
            atomic_store(&mqtt_connected_s, true);
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            atomic_store(&mqtt_connected_s, false);
            break;
        default:
            break;
    }
}

void mqtt_connect(mqtt_handle_t *handle) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_ESP_MQTT_BROKER_URI,
        .credentials = {.username = CONFIG_ESP_MQTT_USERNAME,
                        .authentication.password = CONFIG_ESP_MQTT_PASSWORD}};

    handle->client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(handle->client, ESP_EVENT_ANY_ID,
        mqtt_event_handler, NULL);

    esp_mqtt_client_start(handle->client);

    ESP_LOGI(TAG, "Connecting MQTT ...");
    while(!atomic_load(&mqtt_connected_s)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t mqtt_send_temperature(mqtt_handle_t *handle, float temperature) {
    if (!atomic_load(&mqtt_connected_s)) {
        return ESP_ERR_MQTT_PUBLISH;
    }

    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temperature);

    int msg_id = esp_mqtt_client_publish(handle->client, CONFIG_ESP_MQTT_TOPIC,
                                         payload, 0, MQTT_QOS, 0);
     if (msg_id < 0) {
        return ESP_ERR_MQTT_PUBLISH;
    }

    return ESP_OK;
}
