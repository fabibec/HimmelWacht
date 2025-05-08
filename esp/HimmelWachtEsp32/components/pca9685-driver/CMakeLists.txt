idf_component_register(
    SRCS "pca9685-driver.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_i2c
)
