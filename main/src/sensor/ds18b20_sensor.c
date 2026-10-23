#include "ds18b20_sensor.h"

#include "esp_check.h"
#include <sdkconfig.h>

#include "event_group_bits.h"

// 1byte ROM command + 8byte ROM number + 1byte device command
#define DS18B20_SCRATCHPAD_SIZE 10
static const char *TAG = "SENSOR";

esp_err_t sensor_init(sensor_handle_t *handle) {
    onewire_bus_config_t bus_config = {
            .bus_gpio_num = CONFIG_TELEMETRY_SENSOR_GPIO};
    onewire_bus_rmt_config_t rmt_config = {
            .max_rx_bytes = DS18B20_SCRATCHPAD_SIZE};

    esp_err_t ret = ESP_OK;
    onewire_device_iter_handle_t iter = NULL;
    ds18b20_config_t ds_cfg = {};
    onewire_device_t next_onewire_device;

    if (!handle->bus) {
        ESP_LOGE(TAG, "create bus");
        ESP_RETURN_ON_ERROR(
                onewire_new_bus_rmt(&bus_config, &rmt_config, &handle->bus),
                TAG, "New bus RMT error");
    } else {
        ESP_LOGE(TAG, "bus already exists");
    }

    ESP_GOTO_ON_ERROR(onewire_new_device_iter(handle->bus, &iter), cleanup, TAG,
            "New device iterator error");
    ESP_GOTO_ON_ERROR(onewire_device_iter_get_next(iter, &next_onewire_device),
            cleanup, TAG, "Next device error");

    if (handle->device) {
        ESP_ERROR_CHECK(ds18b20_del_device(handle->device));
        handle->device = NULL;
    }
    ESP_GOTO_ON_ERROR(ds18b20_new_device_from_enumeration(
                              &next_onewire_device, &ds_cfg, &handle->device),
            cleanup, TAG, "New device error");
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));

    handle->sensor_ready = true;
    return ESP_OK;

cleanup:
    if (iter) {
        ESP_ERROR_CHECK(onewire_del_device_iter(iter));
    }
    return ret;
}

esp_err_t sensor_read_temperature(sensor_handle_t *handle, float *temperature) {
    if (!handle->sensor_ready) {
        ESP_RETURN_ON_ERROR(sensor_init(handle), TAG, "Sensor init error");
    }

    esp_err_t err = ds18b20_trigger_temperature_conversion_for_all(handle->bus);
    if (err == ESP_OK) {
        err = ds18b20_get_temperature(handle->device, temperature);
    }

    if (err != ESP_OK) {
        handle->sensor_ready = false;
    }

    return ESP_OK;
}
