#include "clap/entry.h"
#include "sine-ping.h"
#define PLUGINS_COUNT 1

// ye, namespaces are good sometimes
extern const clap_plugin_entry_t sine_ping_plugin;

const clap_plugin_entry_t *plugins[PLUGINS_COUNT] = {&sine_ping_plugin};