idf_component_register(
    SRCS "fire-control.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "pca9685-driver" "flywheel-driver"
)
