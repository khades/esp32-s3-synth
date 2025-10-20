#include "midi-events.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

// weeell i'd rather have streamBuffer, but that list can contain different size of values
void event_list_clear(struct event_list_container *container) {
  struct event_element *pointer = container->first;
  while (pointer != NULL) {
    struct event_element *nextPointer = pointer->next;

    // we need to clear event itself and element after
    free(pointer->event);
    free(pointer);

    pointer = nextPointer;
  }

  container->count = 0;
  container->first = NULL;
  container->last = NULL;
}

// i expect stack variable so i copy it
void event_add(struct event_list_container *container, void *event) {
  clap_event_header_t *eventWrapperd = event;

  clap_event_header_t *newEvent = malloc(eventWrapperd->size);
  memcpy(newEvent, event, eventWrapperd->size);

  struct event_element *newElement = malloc(sizeof(struct event_element));
  newElement->event = newEvent;
  newElement->next = NULL;

  if (container->last != NULL) {
    container->last->next = newElement;
  }
  if (container->first == NULL) {
    container->first = newElement;
  }
  container->last = newElement;
  container->count = container->count + 1;
}

uint32_t event_list_size(const struct clap_input_events *list) {
  struct event_list_container *container = list->ctx;

  return container->count;
};

// Ooh this is sad, we need an covnersion of list to array to get fast indexing
const clap_event_header_t *event_get(const struct clap_input_events *list,
                                     uint32_t index) {
  struct event_list_container *container = list->ctx;
  struct event_element *pointer = container->first;
  uint32_t curr = 0;
  while (pointer != NULL) {
    if (curr == index) {
      ESP_LOGD("Events", "We got curr %d", curr);
      ESP_LOGD("Event", "Pointer is %p", pointer->event);
      return pointer->event;
    }
    pointer = pointer->next;
    curr = curr + 1;
  };
  return NULL;
}

bool try_push(const struct clap_output_events *list,
              const clap_event_header_t *event) {
  struct event_list_container *container = list->ctx;
  ESP_LOGD("Event", "Output pointer is %p", list->ctx);

  event_add(container, (void *)event);

  return true;
};
