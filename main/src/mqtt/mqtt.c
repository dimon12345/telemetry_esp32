#include "mqtt.h"

#include <stdatomic.h>
#include <stdio.h>

#include <esp_log.h>
#include <freertos/idf_additions.h>

#include "event_group_bits.h"

static const char *TAG = "MQTT";

static void mqtt_control_task(void *arg) {
    bool mqtt_started = false;
    mqtt_handle_t *handle = (mqtt_handle_t *)arg;

    for (;;) {
        EventBits_t bits = xEventGroupGetBits(handle->event_group);
        if (bits & WIFI_CONNECTED_BIT) {
            if (!mqtt_started) {
                esp_mqtt_client_start(handle->client);
                mqtt_started = true;
            }
            vTaskDelay(
                    pdMS_TO_TICKS(1000 * CONFIG_ESP_MQTT_LOST_WIFI_SLEEP_TIME));
        } else {
            if (mqtt_started) {
                esp_mqtt_client_stop(handle->client);
                mqtt_started = false;
            }

            xEventGroupWaitBits(handle->event_group, WIFI_CONNECTED_BIT,
                    pdFALSE, pdTRUE, portMAX_DELAY);
        }
    }
}

static void mqtt_event_handler(
        void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    mqtt_handle_t *handle = (mqtt_handle_t *)arg;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type ==
                MQTT_ERROR_TYPE_CONNECTION_REFUSED) {

            esp_mqtt_connect_return_code_t return_code =
                    event->error_handle->connect_return_code;
            if (return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME ||
                    return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) {
                ESP_LOGE(TAG,
                        "Authorization error: Bad user or password "
                        "(Return code: %d)",
                        return_code);
            }
        }
        break;

    case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(handle->event_group, MQTT_CONNECTED_BIT);
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(handle->event_group, MQTT_CONNECTED_BIT);
        break;
    default:
        break;
    }
}

esp_err_t mqtt_init(mqtt_handle_t *handle) {
    esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = CONFIG_ESP_MQTT_BROKER_URI,
            .credentials = {.username = CONFIG_ESP_MQTT_USERNAME,
                    .authentication.password = CONFIG_ESP_MQTT_PASSWORD}};

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Can't init MQTT client");
        return ESP_FAIL;
    }
    handle->client = client;

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
            handle->client, ESP_EVENT_ANY_ID, mqtt_event_handler, handle));

    BaseType_t result =
            xTaskCreate(mqtt_control_task, "mqtt_ctrl", 3072, handle, 3, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Can't create task. (result: %d)", result);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t mqtt_send_temperature(mqtt_handle_t *handle, float temperature) {
    EventBits_t bits = xEventGroupGetBits(handle->event_group);
    if ((bits & MQTT_CONNECTED_BIT) == 0) {
        return ESP_FAIL;
    }

    char payload[16];
    snprintf(payload, sizeof(payload), "%.2f", temperature);

    int msg_id = esp_mqtt_client_publish(handle->client, CONFIG_ESP_MQTT_TOPIC,
            payload, 0, CONFIG_ESP_MQTT_QOS, 0);
    if (msg_id < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
