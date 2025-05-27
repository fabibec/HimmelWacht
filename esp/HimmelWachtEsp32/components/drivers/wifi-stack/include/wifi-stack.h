#pragma once

#include "esp_err.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"

esp_err_t wifi_stack_init(char* wifi_ssid, char* wifi_password);

esp_err_t wifi_stack_deinit(void);
