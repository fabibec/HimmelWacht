/**
 * @file log_wrapper.h
 * @brief Logging wrapper for ESP-IDF
 * 
 * This file provides a simple logging interface that can be enabled or disabled
 * based on the ENABLE_DEBUG_LOGS macro. It uses ESP-IDF's logging functions when
 * enabled, and does nothing when disabled. The set must happen in CMakeLists.txt of the component.
 * Inspired by ChatGPT's suggestion for a logging wrapper.
 * 
 * Usage:
 * - Include this header file in your source files.
 * - Use LOGI, LOGW, and LOGE macros to log information, warnings, and errors respectively.
 * - Define ENABLE_DEBUG_LOGS in your CMakeLists.txt to enable logging
 * 
 * @author Michael Specht
 */

#pragma once

#ifdef ENABLE_DEBUG_LOGS
    #include "esp_log.h"
    #define LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
    #define LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
    #define LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#else
    #define LOGI(tag, fmt, ...)
    #define LOGW(tag, fmt, ...)
    #define LOGE(tag, fmt, ...)
#endif
