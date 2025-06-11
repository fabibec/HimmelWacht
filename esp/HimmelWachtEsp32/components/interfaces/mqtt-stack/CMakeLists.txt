idf_component_register(SRCS "mqtt-stack.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt esp_wifi json utils)

#ENABLE_DEBUG_LOGS                    
target_compile_definitions(${COMPONENT_LIB} PRIVATE)