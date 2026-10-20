#include "ds18b20_sensor.h"

#include "esp_check.h"
#include <sdkconfig.h>

// 1byte ROM command + 8byte ROM number + 1byte device command
#define DS18B20_SCRATCHPAD_SIZE 10

#define TAG "SENSOR"

void init_sensor(sensor_handle_t *handle) {
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = CONFIG_ESP_SENSOR_GPIO,
        .flags = {
            .en_pull_up = true,
        }
    };

    onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes =
                                               DS18B20_SCRATCHPAD_SIZE};
    onewire_device_iter_handle_t iter = NULL;
    ds18b20_config_t ds_cfg = {};
    onewire_device_t next_onewire_device;

    ESP_ERROR_CHECK(
        onewire_new_bus_rmt(&bus_config, &rmt_config, &handle->bus));

    ESP_ERROR_CHECK(onewire_new_device_iter(handle->bus, &iter));
    ESP_ERROR_CHECK(onewire_device_iter_get_next(iter, &next_onewire_device));
    ESP_ERROR_CHECK(ds18b20_new_device_from_enumeration(
        &next_onewire_device, &ds_cfg, &handle->device));
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
}

esp_err_t read_temperature(sensor_handle_t *handle, float *temperature) {
    ESP_RETURN_ON_ERROR(
        ds18b20_trigger_temperature_conversion_for_all(handle->bus), TAG,
        "Trigger temperature conversion error");
    ESP_RETURN_ON_ERROR(ds18b20_get_temperature(handle->device, temperature),
                        TAG, "Get temperature error");

    return ESP_OK;
}
