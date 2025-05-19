idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES "ds4-driver" "platform-control" "fire-control" "manual-control" "motor-driver" "diff-drive"
)
