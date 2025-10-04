 idf_component_register(SRCS  "usb_host_port_main.c" "usbmidi.c" 
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES usb esp_driver_gpio esp_driver_i2s
                    ) 

# idf_component_register(SRCS "blink_example_main.c" 
#     INCLUDE_DIRS "."
#                       PRIV_REQUIRES esp_driver_i2s esp_driver_gpio )