/**
 * @file wifi-stack.h
 * @brief Wi-Fi Stack for ESP32
 * 
 * This header file provides the interface for initializing and deinitializing the Wi-Fi stack on the ESP32.
 * It includes functions to connect to a Wi-Fi network using SSID and password.
 * It also handles the necessary configurations and event handling for Wi-Fi operations.
 * 
 * The code was adapted from the ESP-IDF Wi-Fi example.
 * 
 * Reference:
 *  - https://developer.espressif.com/blog/getting-started-with-wifi-on-esp-idf/#part-2-using-the-wi-fi-apis-1
 * 
 * @author Michael Specht
 */

#pragma once

#include "esp_err.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"

/**
 * @brief Initialize the Wi-Fi stack and connect to a Wi-Fi network.
 * 
 * @param wifi_ssid Pointer to the Wi-Fi SSID string.
 * @param wifi_password Pointer to the Wi-Fi password string.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t wifi_stack_init(char* wifi_ssid, char* wifi_password);

/**
 * @brief Deinitialize the Wi-Fi stack and disconnect from the Wi-Fi network.
 * 
 * This function cleans up the Wi-Fi resources, stops the Wi-Fi driver, and unregisters event handlers.
 * 
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t wifi_stack_deinit(void);
