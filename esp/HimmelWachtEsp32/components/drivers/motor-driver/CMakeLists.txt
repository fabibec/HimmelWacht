idf_component_register(
    SRCS "motor-driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos utils
)

# Enable logging only for this component
target_compile_definitions(${COMPONENT_LIB} PRIVATE)