idf_component_register(SRCS "diff-drive.c"
                       INCLUDE_DIRS "include"
                       REQUIRES motor-driver freertos driver utils)

# Enable logging only for this component ENABLE_DEBUG_LOGS
target_compile_definitions(${COMPONENT_LIB} PRIVATE )