# for c++ synth
#  idf_component_register(SRCS "clap-saw-demo/src/saw-voice.cpp" "clap-saw-demo/src/clap-saw-demo.cpp" "clap-saw-demo/src/clap-saw-demo-pluginentry.cpp" "usb-midi-task.c" "audio-task.c" "synth-task.c"  "usb-midi.c"  "plugin-host.c" "synth_main.c" "midi-events.c"  
#                     INCLUDE_DIRS "." "clap/include" "clap-helpers/include" "readerwriterqueue"
#                     PRIV_REQUIRES usb esp_driver_gpio esp_driver_i2s
#                     ) 

idf_component_register(SRCS "usb-midi-task.c" "audio-task.c" "synth-task.c"  "sine-ping/Parameters.c" "sine-ping/Plugin.c" "sine-ping/Stream.c" "sine-ping/Voice.c" "sine-ping/sine-ping.c" "usb-midi.c"  "plugin-host.c" "synth_main.c" "midi-events.c"  
                    INCLUDE_DIRS "." "clap/include" 
                    PRIV_REQUIRES usb esp_driver_gpio esp_driver_i2s
                    ) 