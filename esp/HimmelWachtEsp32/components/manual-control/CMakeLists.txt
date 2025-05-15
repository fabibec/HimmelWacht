idf_component_register(
    SRCS "manual-control.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "ds4-driver" "platform-control" "fire-control"
)
