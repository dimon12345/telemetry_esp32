#include "ds18b20_sensor.h"

#include <assert.h>
#include <stdint.h>

#include <esp_check.h>
#include <esp_task_wdt.h>
#include <sdkconfig.h>

#include <driver/gpio.h>

#include "event_group_bits.h"

// 1 byte ROM command + 8 byte ROM number + 1 byte device command
#define DS18B20_MAX_RX_BYTES 10
static const char *TAG = "SENSOR";

_Static_assert(GPIO_IS_VALID_OUTPUT_GPIO(CONFIG_TELEMETRY_SENSOR_GPIO),
        "Sensor GPIO must support input/output.");

static esp_err_t sensor_init(sensor_handle_t *handle) {
    assert(handle != NULL);

    onewire_bus_config_t bus_config = {
            .bus_gpio_num = CONFIG_TELEMETRY_SENSOR_GPIO};
    onewire_bus_rmt_config_t rmt_config = {
            .max_rx_bytes = DS18B20_MAX_RX_BYTES};

    esp_err_t ret = ESP_OK;
    onewire_device_iter_handle_t iter = NULL;
    ds18b20_config_t ds_cfg = {};
    onewire_device_t next_onewire_device;

    if (!handle->bus) {
        ESP_LOGI(TAG, "Creating 1-Wire bus on GPIO %d",
                CONFIG_TELEMETRY_SENSOR_GPIO);
        ESP_RETURN_ON_ERROR(
                onewire_new_bus_rmt(&bus_config, &rmt_config, &handle->bus),
                TAG, "New bus RMT error");
    }

    ESP_GOTO_ON_ERROR(onewire_new_device_iter(handle->bus, &iter), cleanup, TAG,
            "New device iterator error");
    ESP_GOTO_ON_ERROR(onewire_device_iter_get_next(iter, &next_onewire_device),
            cleanup, TAG, "Next device error");

    if (handle->device) {
        esp_err_t del_err = ds18b20_del_device(handle->device);
        handle->device = NULL;
        if (del_err != ESP_OK) {
            ESP_LOGW(TAG, "Delete sensor device error: %s (0x%X)",
                    esp_err_to_name(del_err), del_err);
        }
    }
    ESP_GOTO_ON_ERROR(ds18b20_new_device_from_enumeration(
                              &next_onewire_device, &ds_cfg, &handle->device),
            cleanup, TAG, "New device error");

    handle->sensor_ready = true;

cleanup:
    if (iter) {
        esp_err_t del_err = onewire_del_device_iter(iter);
        if (del_err != ESP_OK) {
            ESP_LOGW(TAG, "Delete iterator error: %s (0x%X)",
                    esp_err_to_name(del_err), del_err);
        }
    }
    return ret;
}

esp_err_t sensor_read_temperature(sensor_handle_t *handle, float *temperature) {
    assert(handle != NULL);
    assert(temperature != NULL);

    EventBits_t bits = xEventGroupGetBits(event_group_g);
    if ((bits & TEMPERATURE_READY_BIT) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupClearBits(event_group_g, TEMPERATURE_READY_BIT);

    *temperature = handle->temperature;
    return ESP_OK;
}

void sensor_update(sensor_handle_t *handle, TickType_t elapsed) {
    assert(handle != NULL);

    xEventGroupClearBits(event_group_g, TEMPERATURE_READY_BIT);

    esp_err_t err = backoff_update(&handle->backoff, (uint64_t)elapsed);
    if (err == ESP_ERR_INVALID_STATE) {
        return;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor backoff error: %s (0x%X)", esp_err_to_name(err),
                err);
        return;
    }

    if (!handle->sensor_ready) {
        err = sensor_init(handle);
        if (err != ESP_OK) {
            backoff_on_error(&handle->backoff);

            ESP_LOGE(TAG, "Sensor init error: %s (0x%X)", esp_err_to_name(err),
                    err);
            return;
        }
    }

    handle->sensor_ready = true;

    err = ds18b20_trigger_temperature_conversion_for_all(handle->bus);
    if (err != ESP_OK) {
        handle->sensor_ready = false;
        backoff_on_error(&handle->backoff);
        ESP_LOGE(TAG, "Sensor conversion error: %s (0x%X)",
                esp_err_to_name(err), err);
        return;
    }

    err = ds18b20_get_temperature(handle->device, &handle->temperature);
    if (err != ESP_OK) {
        handle->sensor_ready = false;
        backoff_on_error(&handle->backoff);
        ESP_LOGE(TAG, "Sensor get temperature error: %s (0x%X)",
                esp_err_to_name(err), err);
        return;
    }

    backoff_on_success(&handle->backoff);
    xEventGroupSetBits(event_group_g, TEMPERATURE_READY_BIT);
}
