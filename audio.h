#ifndef __audio__
#define __audio__
#include "FreeRTOSConfig.h"
#include "hal/i2s_types.h"
#define SAMPLE_RATE 48000

// In ideal world that should be 1
#define BUFFER_MULTIPLIER 64
// we need this amount of samples per cycle
#define SAMPLES_PER_TICK (SAMPLE_RATE * BUFFER_MULTIPLIER / configTICK_RATE_HZ)

#define AUDIO_BITS I2S_DATA_BIT_WIDTH_32BIT
#define AUDIO_CHANNELS I2S_SLOT_MODE_STEREO
// DMA FRAMENUM
// funnily coincides with Samples per tick
#define FRAME_NUMBERS SAMPLES_PER_TICK
// dma_desc_num
// funnily coincides with amount of channels
#define BUFFERS AUDIO_CHANNELS

// should be 512 bytes
// #define DMA_BUFFER_SIZE (AUDIO_BITS / 8 * BUFFERS * FRAME_NUMBERS)

#endif