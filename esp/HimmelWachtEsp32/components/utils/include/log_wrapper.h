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
