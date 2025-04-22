idf_component_register(
    SRCS "main.c" "ds4-driver/ds4-platform.c"
    INCLUDE_DIRS "."
    REQUIRES "bluepad32" "btstack"
    # PRIV_REQUIRES
)
