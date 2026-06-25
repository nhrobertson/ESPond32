idf_component_register(SRCS "io.c"
                       INCLUDE_DIRS "include"
                       REQUIRES models
                       PRIV_REQUIRES esp_driver_gpio 
)
