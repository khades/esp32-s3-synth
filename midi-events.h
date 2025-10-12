#include "clap/events.h"

#ifndef __MIDI_EVENTS
#define __MIDI_EVENTS

struct event_element {
  clap_event_header_t *event;
  struct event_element *next;
};

struct event_list_container {
  int count;
  struct event_element *first;
  struct event_element *last;
};

void event_list_clear(struct event_list_container *container);

void event_add(struct event_list_container *container, void *event);

uint32_t event_list_size(const struct clap_input_events *list);

const clap_event_header_t *event_get(const struct clap_input_events *list,
                                     uint32_t index);

bool try_push(const struct clap_output_events *list,
              const clap_event_header_t *event);
#endif