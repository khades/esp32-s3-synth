#ifndef __audio__
#define __audio__

#include "driver/i2s_common.h"

#define SAMPLE_RATE 48000
#define AUDIO_BITS I2S_DATA_BIT_WIDTH_32BIT
#define AUDIO_CHANNELS I2S_SLOT_MODE_STEREO
#define BUFFERS 2
#define BUFFER_SIZE 32
#define FRAME_SIZE (BUFFERS * BUFFER_SIZE)
#define FRAME_SIZE_FOR_ALL_CHANNELS (FRAME_SIZE * AUDIO_CHANNELS)

#define CHANNEL_BUFFER_BYTES (AUDIO_BITS / 8 * BUFFERS * BUFFER_SIZE)
#endif