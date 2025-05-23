idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "../components/drivers" "../components/interfaces"
    REQUIRES "ds4-driver" "platform-control" "fire-control" "vehicle-control" "motor-driver" "diff-drive" "pca9685-driver"
)
