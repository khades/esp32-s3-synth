#ifndef __audio__
#define __audio__

#include "driver/i2s_common.h"

// EVERYTHING IS WRONG GO READ
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html#application-notes
#define SAMPLE_RATE 48000
#define AUDIO_BITS I2S_DATA_BIT_WIDTH_32BIT
#define AUDIO_CHANNELS I2S_SLOT_MODE_STEREO
// DMA FRAMENUM
// for samplerate of 48000 and buffersize of latency (or interrupt rate) should
// be 1.3ms
#define FRAME_NUMBERS 64
// dma_desc_num
// should be more than tickrate / interrupt, which is 1/1.3
#define BUFFERS 2

// should be 512 bytes
#define DMA_BUFFER_SIZE (AUDIO_BITS / 8 * BUFFERS * FRAME_NUMBERS)

// in bits
#define STEREOFRAMES_TO_RENDER (FRAME_NUMBERS * BUFFERS)

#define MONO_FRAMES_TO_RENDER (FRAME_NUMBERS * BUFFERS / AUDIO_CHANNELS)

#endif