idf_component_register(
    SRCS "pca9685-driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
)
