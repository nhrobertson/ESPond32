idf_component_register(SRCS "espond32.c"
                      INCLUDE_DIRS "."
                      REQUIRES config led
                      PRIV_REQUIRES esp_driver_gpio
)
