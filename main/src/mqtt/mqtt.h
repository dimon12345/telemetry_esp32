#pragma once

#include "esp_err.h"

esp_err_t mqtt_connect();
void mqtt_send_temperature(float temperature);
