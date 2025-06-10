idf_component_register(
    SRCS "vehicle-control.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "ds4-driver" "platform-control" "fire-control" "espressif__iqmath" "diff-drive" "mqtt-stack"
)
