#include "clap/plugin.h"

typedef struct {
  const clap_plugin_descriptor_t *descriptor;
  const clap_plugin_t *plugin;
} plugin_entry_t;

void plugins_init();
void plugins_activate(double sample_rate);
void plugins_process(clap_input_events_t *in_events, float **output);