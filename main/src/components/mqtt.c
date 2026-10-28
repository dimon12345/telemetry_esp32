#include "mqtt.h"

#include <assert.h>
#include <stdio.h>

#include "esp_err.h"
#include <esp_check.h>
#include <esp_log.h>
#include <esp_system.h>

#include <mqtt_client.h>

#include "event_group_bits.h"
#include "util/backoff_timer.h"

#define MQTT_QOS 0
static const char *TAG = "MQTT";

static const uint64_t MESSAGE_INTERVAL_TICKS =
        (uint64_t)CONFIG_TELEMETRY_SENSOR_MESSAGE_TIME_INTERVAL *
        (uint64_t)pdMS_TO_TICKS(1000U);

_Static_assert(
        sizeof(CONFIG_TELEMETRY_MQTT_USERNAME) > 1, "MQTT username is empty");
_Static_assert(
        sizeof(CONFIG_TELEMETRY_MQTT_PASSWORD) > 1, "MQTT password is empty");

static void mqtt_event_handler(
        void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_ERROR:
        switch (event->error_handle->error_type) {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            esp_mqtt_error_codes_t *error = event->error_handle;
            int socket_error = error->esp_transport_sock_errno;
            int tls_last_error = error->esp_tls_last_esp_err;

            if (tls_last_error != ESP_OK) {
                ESP_LOGE(TAG, "MQTT transport error: %s (0x%X)",
                        esp_err_to_name(tls_last_error), tls_last_error);
            }

            if (socket_error != 0) {
                ESP_LOGE(TAG, "Socket error: %d", socket_error);
            }

            if (tls_last_error == ESP_OK && socket_error == 0) {
                ESP_LOGE(TAG, "TCP transport error");
            }

            break;
        case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
            esp_mqtt_connect_return_code_t return_code =
                    event->error_handle->connect_return_code;
            if (return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME ||
                    return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) {
                ESP_LOGE(TAG,
                        "Authorization error: Bad user or password "
                        "(Return code: %d)",
                        return_code);
                break;
            }
            ESP_LOGE(TAG, "MQTT event error with type: %d and returncode: %d",
                    event->error_handle->error_type, return_code);
            break;
        default:
            ESP_LOGE(TAG, "MQTT event error with type: %d",
                    event->error_handle->error_type);
            break;
        }
        break;

    case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(event_group_g, MQTT_CONNECTED_BIT | MQTT_SEND_BIT);
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(event_group_g, MQTT_CONNECTED_BIT);
        break;
    default:
        break;
    }
}

static esp_err_t mqtt_init(mqtt_handle_t *handle) {
    assert(handle != NULL);

    handle->remaining_ticks = 0ULL;

    esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = CONFIG_TELEMETRY_MQTT_BROKER_URI,
            .credentials = {.username = CONFIG_TELEMETRY_MQTT_USERNAME,
                    .authentication.password = CONFIG_TELEMETRY_MQTT_PASSWORD}};

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Can't init MQTT client");
        return ESP_FAIL;
    }
    handle->client = client;

    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(handle->client,
                                ESP_EVENT_ANY_ID, mqtt_event_handler, NULL),
            TAG, "MQTT client register event error");

    ESP_RETURN_ON_ERROR(esp_mqtt_client_start(handle->client), TAG,
            "MQTT client start error");

    return ESP_OK;
}

static esp_err_t mqtt_send_temperature(mqtt_handle_t *handle) {
    assert(handle != NULL);

    if (!handle->inited) {
        EventBits_t bits = xEventGroupGetBits(event_group_g);
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err = mqtt_init(handle);
        if (err != ESP_OK) {
            return err;
        }
        handle->inited = true;
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupGetBits(event_group_g);
    if ((bits & MQTT_CONNECTED_BIT) == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    char payload[32];
    int len = snprintf(payload, sizeof(payload), "{\"temperature\":%.4f}",
            handle->temperature);
    if (len < 0 || len >= (int)sizeof(payload)) {
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(handle->client,
            CONFIG_TELEMETRY_MQTT_TOPIC, payload, 0, MQTT_QOS, 0);
    if (msg_id < 0) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sent MQTT message %s", payload);

    return ESP_OK;
}

void mqtt_update(mqtt_handle_t *handle, TickType_t elapsed) {
    assert(handle != NULL);

    if (xEventGroupGetBits(event_group_g) & MQTT_SEND_BIT) {
        xEventGroupClearBits(event_group_g, MQTT_SEND_BIT);
        handle->remaining_ticks = 0;
        backoff_on_success(&handle->backoff);
    }

    esp_err_t err = backoff_update(&handle->backoff, (uint64_t)elapsed);
    if (err == ESP_ERR_INVALID_STATE) {
        return;
    }

    if ((uint64_t)elapsed < handle->remaining_ticks) {
        handle->remaining_ticks -= elapsed;
        return;
    }

    EventBits_t bits = xEventGroupGetBits(event_group_g);
    if ((bits & TEMPERATURE_SEND_PENDING) == 0) {
        return;
    }

    if (mqtt_send_temperature(handle) == ESP_OK) {
        backoff_on_success(&handle->backoff);
        handle->remaining_ticks = MESSAGE_INTERVAL_TICKS;
        xEventGroupClearBits(
                event_group_g, TEMPERATURE_SEND_PENDING | RESTART_TIMEOUT_BIT);
    } else {
        backoff_on_error(&handle->backoff);
        handle->remaining_ticks = 0;
    }
}

void mqtt_temperature_for_send(mqtt_handle_t *handle, float temperature) {
    assert(handle != NULL);

    handle->temperature = temperature;
    xEventGroupSetBits(event_group_g, TEMPERATURE_SEND_PENDING);
}
