#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"

esp_err_t wifi_connect();
bool wifi_is_connected();
