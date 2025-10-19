 idf_component_register(SRCS "audio-task.c" "sine-ping/Parameters.c" "sine-ping/Plugin.c" "sine-ping/Stream.c" "sine-ping/Voice.c" "sine-ping.c" "usb-midi.c"  "plugin-host.c" "synth_main.c" "midi-events.c"  
                    INCLUDE_DIRS "." "clap/include"
                    PRIV_REQUIRES usb esp_driver_gpio esp_driver_i2s
                    ) 

# idf_component_register(SRCS "synth_main.c" "blink_example_main.c" 
#     INCLUDE_DIRS "."
#                       PRIV_REQUIRES esp_driver_i2s esp_driver_gpio )