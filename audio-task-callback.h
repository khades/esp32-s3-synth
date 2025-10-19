#ifndef __AUDIO_TASK_CALLBACK
#define __AUDIO_TASK_CALLBACK
// #include "audio-task.h"
#include "driver/i2s_types.h"

bool audioTaskCallback(i2s_chan_handle_t handle, i2s_event_data_t *event,
                       void *user_ctx);
#endif