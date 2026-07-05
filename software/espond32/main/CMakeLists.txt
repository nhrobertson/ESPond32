idf_component_register(SRCS "espond32.c"
                      INCLUDE_DIRS "."
                      REQUIRES models config device filesystem tasks network
                      PRIV_REQUIRES esp_driver_gpio nvs_flash 
)
