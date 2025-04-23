idf_component_register(
    SRCS "ds4-driver.c" "ds4-platform.c" "ds4-common.c"
    INCLUDE_DIRS "include"
    REQUIRES "bluepad32" "btstack"
)
