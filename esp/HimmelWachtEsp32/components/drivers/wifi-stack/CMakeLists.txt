idf_component_register(SRCS "wifi-stack.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "nvs_flash" "esp_event" "esp_wifi" "freertos" "esp_netif")
