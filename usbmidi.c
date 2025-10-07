/*
 * ESP32 USB MIDI Host Library (ESP32 USB MIDI Omocha)
 * Copyright (c) 2025 ndenki
 * https://github.com/enudenki/esp32-usb-host-midi-library.git
 * That's naive port to C from a person that doesnt know either C or C++
 */

#include "usbmidi.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "UST_MIDI_DRIVER";

static void _clientEventCallback(const usb_host_client_event_msg_t *eventMsg,
                                 void *arg);
static void _midiTransferCallback(usb_transfer_t *transfer);

static void _handleClientEvent(struct UsbMidi *midi,
                               const usb_host_client_event_msg_t *eventMsg);
static void _handleMidiTransfer(struct UsbMidi *midi, usb_transfer_t *transfer);
static void _parseConfigDescriptor(struct UsbMidi *midi,
                                   const usb_config_desc_t *configDesc);
static void _findAndClaimMidiInterface(struct UsbMidi *midi,
                                       const usb_intf_desc_t *intf);
static void _setupMidiEndpoints(struct UsbMidi *midi,
                                const usb_ep_desc_t *endpoint);
static void _setupMidiInEndpoint(struct UsbMidi *midi,
                                 const usb_ep_desc_t *endpoint);
static void _setupMidiOutEndpoint(struct UsbMidi *midi,
                                  const usb_ep_desc_t *endpoint);
static void _releaseDeviceResources(struct UsbMidi *midi);
static void _processMidiOutQueue(struct UsbMidi *midi);
static size_t _getQueueAvailableSize(const struct UsbMidi *midi);

void UsbMidiInit(struct UsbMidi *midi) {

  // Initialize all members to their default values
  midi->_clientHandle = NULL;
  midi->_deviceHandle = NULL;
  midi->_midiOutTransfer = NULL;
  midi->_midiOutQueue = NULL;
  midi->_midiInterfaceNumber = 0;
  midi->_isMidiInterfaceFound = false;
  midi->_areEndpointsReady = false;
  midi->_isMidiOutBusy = false;
  midi->_midiMessageCallback = NULL;
  midi->_deviceConnectedCallback = NULL;
  midi->_deviceDisconnectedCallback = NULL;

  // Initialize the array of MIDI In transfers
  for (int i = 0; i < NUM_MIDI_IN_TRANSFERS; ++i) {
    midi->_midiInTransfers[i] = NULL;
  }
}

void usb_midi_destroy(struct UsbMidi *midi) {
  _releaseDeviceResources(midi);
  if (midi->_clientHandle) {
    usb_host_client_deregister(midi->_clientHandle);
  }
  usb_host_uninstall();
  if (midi->_midiOutQueue) {
    vQueueDelete(midi->_midiOutQueue);
  }
}

void usb_midi_begin(struct UsbMidi *midi) {
  midi->_midiOutQueue = xQueueCreate(MIDI_OUT_QUEUE_SIZE, sizeof(uint8_t[4]));
  if (!midi->_midiOutQueue) {
    ESP_LOGE(TAG, "Failed to create MIDI OUT queue\n");
  }

  const usb_host_config_t hostConfig = {
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  esp_err_t err = usb_host_install(&hostConfig);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "USB Host installed\n");
  } else {
    ESP_LOGE(TAG, "usb_host_install failed: 0x%x\n", err);
  }

  const usb_host_client_config_t clientConfig = {
      .is_synchronous = false,
      .max_num_event_msg = MAX_CLIENT_EVENT_MESSAGES,
      // was this here
      .async = {.client_event_callback = _clientEventCallback,
                .callback_arg = midi}};
  err = usb_host_client_register(&clientConfig, &midi->_clientHandle);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "USB Host client registered\n");
  } else {
    ESP_LOGE(TAG, "usb_host_client_register failed: 0x%x\n", err);
  }
}

void UsbMidi_update(struct UsbMidi *midi) {
  esp_err_t err;

  err =
      usb_host_client_handle_events(midi->_clientHandle, USB_EVENT_POLL_TICKS);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Error in usb_host_client_handle_events(): 0x%x\n", err);
  }
  err = usb_host_lib_handle_events(USB_EVENT_POLL_TICKS, NULL);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGE(TAG, "Error in usb_host_lib_handle_events(): 0x%x\n", err);
  }

  _processMidiOutQueue(midi);
}

void UsbMidi_onMidiMessage(struct UsbMidi *midi, MidiMessageCallback callback) {
  midi->_midiMessageCallback = callback;
}

bool UsbMidi_sendMidiMessage(struct UsbMidi *midi, const uint8_t *message,
                             uint8_t size) {
  if (!midi->_midiOutQueue)
    return false;
  if (size == 0 || size % 4 != 0)
    return false;

  size_t numMessages = size / 4;

  if (_getQueueAvailableSize(midi) < numMessages) {
    ESP_LOGW(TAG, "Not enough space in MIDI OUT queue. Message dropped.\n");
    return false;
  }

  for (size_t i = 0; i < numMessages; ++i) {
    const uint8_t *currentMessage = message + (i * 4);
    if (xQueueSend(midi->_midiOutQueue, currentMessage, 0) != pdPASS) {
      ESP_LOGE(
          TAG,
          "Critical error: Failed to send to queue despite available space.\n");
      return false;
    }
  }

  return true;
}

bool UsbMidi_noteOn(struct UsbMidi *midi, uint8_t channel, uint8_t note,
                    uint8_t velocity) {
  uint8_t message[4] = {(uint8_t)MIDI_CIN_NOTE_ON,
                        (uint8_t)(0x90 | (channel & 0x0F)),
                        (uint8_t)(note & 0x7F), (uint8_t)(velocity & 0x7F)};
  return UsbMidi_sendMidiMessage(midi, message, 4);
}

bool UsbMidi_noteOff(struct UsbMidi *midi, uint8_t channel, uint8_t note,
                     uint8_t velocity) {
  uint8_t message[4] = {(uint8_t)MIDI_CIN_NOTE_OFF,
                        (uint8_t)(0x80 | (channel & 0x0F)),
                        (uint8_t)(note & 0x7F), (uint8_t)(velocity & 0x7F)};
  return UsbMidi_sendMidiMessage(midi, message, 4);
}

bool UsbMidi_controlChange(struct UsbMidi *midi, uint8_t channel,
                           uint8_t controller, uint8_t value) {
  uint8_t message[4] = {(uint8_t)MIDI_CIN_CONTROL_CHANGE,
                        (uint8_t)(0xB0 | (channel & 0x0F)),
                        (uint8_t)(controller & 0x7F), (uint8_t)(value & 0x7F)};
  return UsbMidi_sendMidiMessage(midi, message, 4);
}

bool UsbMidi_programChange(struct UsbMidi *midi, uint8_t channel,
                           uint8_t program) {
  uint8_t message[4] = {(uint8_t)MIDI_CIN_PROGRAM_CHANGE,
                        (uint8_t)(0xC0 | (channel & 0x0F)),
                        (uint8_t)(program & 0x7F), 0};
  return UsbMidi_sendMidiMessage(midi, message, 4);
}

size_t _getQueueAvailableSize(const struct UsbMidi *midi) {
  if (!midi->_midiOutQueue) {
    return 0;
  }
  return uxQueueSpacesAvailable(midi->_midiOutQueue);
}

void UsbMidi_onDeviceConnected(struct UsbMidi *midi, void (*callback)()) {
  midi->_deviceConnectedCallback = callback;
}

void UsbMidi_onDeviceDisconnected(struct UsbMidi *midi, void (*callback)()) {
  midi->_deviceDisconnectedCallback = callback;
}

void _clientEventCallback(const usb_host_client_event_msg_t *eventMsg,
                          void *arg) {
  if (arg == NULL)
    return;
  struct UsbMidi *instance = (struct UsbMidi *)arg;
  _handleClientEvent(instance, eventMsg);
}

void _handleClientEvent(struct UsbMidi *midi,
                        const usb_host_client_event_msg_t *eventMsg) {
  esp_err_t err;
  switch (eventMsg->event) {
  case USB_HOST_CLIENT_EVENT_NEW_DEV:
    if (midi->_deviceHandle) {
      ESP_LOGI(TAG, "Ignoring new device, one is already connected.\n");
      return;
    }
    ESP_LOGI(TAG, "New device connected (address: %d)\n",
             eventMsg->new_dev.address);

    err = usb_host_device_open(midi->_clientHandle, eventMsg->new_dev.address,
                               &midi->_deviceHandle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to open device: 0x%x\n", err);
      return;
    }

    const usb_config_desc_t *configDesc;
    err =
        usb_host_get_active_config_descriptor(midi->_deviceHandle, &configDesc);
    if (err == ESP_OK) {
      _parseConfigDescriptor(midi, configDesc);
    } else {
      ESP_LOGW(TAG, "Failed to get config descriptor: 0x%x\n", err);
      usb_host_device_close(midi->_clientHandle, midi->_deviceHandle);
      midi->_deviceHandle = NULL;
    }
    break;

  case USB_HOST_CLIENT_EVENT_DEV_GONE:
    if (eventMsg->dev_gone.dev_hdl == midi->_deviceHandle) {
      ESP_LOGI(TAG, "MIDI device disconnected.\n");
      _releaseDeviceResources(midi);
      if (midi->_deviceDisconnectedCallback) {
        midi->_deviceDisconnectedCallback();
      }
    }
    break;

  default:
    break;
  }
}

void _parseConfigDescriptor(struct UsbMidi *midi,
                            const usb_config_desc_t *configDesc) {
  const uint8_t *p = &configDesc->val[0];
  const uint8_t *end = p + configDesc->wTotalLength;
  while (p < end) {
    const uint8_t bLength = p[0];

    if (bLength == 0 || (p + bLength) > end)
      break;

    const uint8_t bDescriptorType = p[1];
    switch (bDescriptorType) {
    case USB_B_DESCRIPTOR_TYPE_INTERFACE:
      ESP_LOGI(TAG, "Found Interface Descriptor\n");
      if (!midi->_isMidiInterfaceFound) {
        _findAndClaimMidiInterface(midi, (usb_intf_desc_t *)p);
      }
      break;
    case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
      ESP_LOGI(TAG, "Found Endpoint Descriptor\n");
      if (midi->_isMidiInterfaceFound && !midi->_areEndpointsReady) {
        _setupMidiEndpoints(midi, (usb_ep_desc_t *)p);
      }
      break;
    default:
      ESP_LOGI(TAG, "Found other descriptor, type: 0x%02X\n", bDescriptorType);
      break;
    }
    p += bLength;
  }
}

void _findAndClaimMidiInterface(struct UsbMidi *midi,
                                const usb_intf_desc_t *intf) {
  if (intf->bInterfaceClass == USB_CLASS_AUDIO &&
      intf->bInterfaceSubClass == USB_AUDIO_SUBCLASS_MIDI_STREAMING) {
    esp_err_t err = usb_host_interface_claim(
        midi->_clientHandle, midi->_deviceHandle, intf->bInterfaceNumber,
        intf->bAlternateSetting);

    if (err == ESP_OK) {
      ESP_LOGI(TAG, "Successfully claimed MIDI interface.\n");
      midi->_midiInterfaceNumber = intf->bInterfaceNumber;
      midi->_isMidiInterfaceFound = true;
    } else {
      ESP_LOGE(TAG, "Failed to claim MIDI interface: 0x%x\n", err);
      midi->_isMidiInterfaceFound = false;
    }
  }
}

void _setupMidiEndpoints(struct UsbMidi *midi, const usb_ep_desc_t *endpoint) {
  ESP_LOGD(TAG, "SETUP: PRESTARTING");

  if ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) !=
      USB_BM_ATTRIBUTES_XFER_BULK)
    return;
  ESP_LOGD(TAG, "SETUP: STARTING");
  if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
    _setupMidiInEndpoint(midi, endpoint);
  } else {
    _setupMidiOutEndpoint(midi, endpoint);
  }

  if (!midi->_areEndpointsReady) {
    ESP_LOGD(TAG, "SETUP: NO ENDPOINTS");
  }
  if (!midi->_midiOutTransfer) {
    ESP_LOGD(TAG, "SETUP: NO MIDITRANSFER");
  }
  if (!midi->_midiInTransfers[0]) {
    ESP_LOGD(TAG, "SETUP: NO MIDI IN TRANSFERS");
  }
  if (!midi->_areEndpointsReady && midi->_midiOutTransfer &&
      midi->_midiInTransfers[0]) {
    midi->_areEndpointsReady = true;
    if (midi->_deviceConnectedCallback)
      midi->_deviceConnectedCallback();
  }
}

void _setupMidiInEndpoint(struct UsbMidi *midi, const usb_ep_desc_t *endpoint) {
  for (int i = 0; i < NUM_MIDI_IN_TRANSFERS; i++) {
    if (midi->_midiInTransfers[i] != NULL)
      continue;

    esp_err_t err = usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0,
                                            &midi->_midiInTransfers[i]);
    if (err == ESP_OK) {
      midi->_midiInTransfers[i]->device_handle = midi->_deviceHandle;
      midi->_midiInTransfers[i]->bEndpointAddress = endpoint->bEndpointAddress;
      midi->_midiInTransfers[i]->callback = _midiTransferCallback;
      midi->_midiInTransfers[i]->context = midi;
      midi->_midiInTransfers[i]->num_bytes = endpoint->wMaxPacketSize;
      usb_host_transfer_submit(midi->_midiInTransfers[i]);
    } else {
      midi->_midiInTransfers[i] = NULL;
    }
  }
}

void _setupMidiOutEndpoint(struct UsbMidi *midi,
                           const usb_ep_desc_t *endpoint) {
  if (midi->_midiOutTransfer != NULL)
    return;

  esp_err_t err = usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0,
                                          &midi->_midiOutTransfer);
  if (err == ESP_OK) {
    midi->_midiOutTransfer->device_handle = midi->_deviceHandle;
    midi->_midiOutTransfer->bEndpointAddress = endpoint->bEndpointAddress;
    midi->_midiOutTransfer->callback = _midiTransferCallback;
    midi->_midiOutTransfer->context = midi;
  } else {
    midi->_midiOutTransfer = NULL;
  }
}

// This callback handles both IN and OUT transfer completions.
// It is always executed in the context of the same, single USB Host Library
// task.
void _midiTransferCallback(usb_transfer_t *transfer) {
  if (transfer == NULL) {
    ESP_LOGD(TAG, "Empty transfer");
    return;
  }

  if (transfer->context == NULL) {
    ESP_LOGD(TAG, "Empty context");

    return;
  }
  struct UsbMidi *instance = (struct UsbMidi *)transfer->context;
  _handleMidiTransfer(instance, transfer);
}

void _handleMidiTransfer(struct UsbMidi *midi, usb_transfer_t *transfer) {
  if (midi->_deviceHandle != transfer->device_handle) {
    return;
  }

  bool isInTransfer =
      (transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK);

  if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
    if (isInTransfer) {
      for (int i = 0; i < transfer->actual_num_bytes; i += 4) {
        uint8_t *const p = &transfer->data_buffer[i];
        uint8_t codeIndexNumber = p[0] & 0x0F;

        // CIN 0 is for miscellaneous system messages, not musical data. We
        // ignore them.
        if (codeIndexNumber != 0 && midi->_midiMessageCallback) {
          midi->_midiMessageCallback(p);
        }
      }
      usb_host_transfer_submit(transfer);
    } else { // OutTransfer
      // Explicitly release the lock for the completed transfer.
      atomic_store(&midi->_isMidiOutBusy, false);

      // Immediately try to process the next batch of messages from the queue
      // to maximize throughput, even if the main loop is slow.
      _processMidiOutQueue(midi);
    }
  } else if (transfer->status != USB_TRANSFER_STATUS_CANCELED) {
    ESP_LOGE(TAG, "MIDI Transfer failed. Endpoint: 0x%02X, Status: %d\n",
             transfer->bEndpointAddress, transfer->status);
    if (isInTransfer) {
      usb_host_transfer_submit(transfer);
    } else {
      // On failure, we must release the lock so new transfers can be attempted.
      atomic_store(&midi->_isMidiOutBusy, false);
    }
  }
}

// This function may be called from both the main task and the USB Host task.
void _processMidiOutQueue(struct UsbMidi *midi) {

  if (!midi->_midiOutTransfer) {
    ESP_LOGD(TAG, "NO MIDI TRANSFER");
    return;
  }
  if (!midi->_areEndpointsReady) {
    ESP_LOGD(TAG, "NOT READY MIDI");
    return;
  }

  bool expected = false;

  if (!atomic_compare_exchange_strong(&midi->_isMidiOutBusy, &expected, true)) {
    return; // Lock acquisition failed, means it's already busy.
  }

  // From this point, we have successfully acquired the "lock" (_isMidiOutBusy
  // is now true).

  if (uxQueueMessagesWaiting(midi->_midiOutQueue) == 0) {
    atomic_store(&midi->_isMidiOutBusy,
                 false); // Nothing to send, release the lock.
    return;
  }

  size_t bytesToSend = 0;
  uint8_t tempMessage[4];
  size_t maxPacketSize = midi->_midiOutTransfer->data_buffer_size;

  while (bytesToSend + 4 <= maxPacketSize &&
         uxQueueMessagesWaiting(midi->_midiOutQueue) > 0) {
    if (xQueueReceive(midi->_midiOutQueue, tempMessage, 0) == pdPASS) {
      memcpy(midi->_midiOutTransfer->data_buffer + bytesToSend, tempMessage, 4);
      bytesToSend += 4;
    }
  }

  if (bytesToSend > 0) {
    midi->_midiOutTransfer->num_bytes = bytesToSend;
    esp_err_t err = usb_host_transfer_submit(midi->_midiOutTransfer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to submit MIDI OUT transfer: 0x%x\n", err);
      atomic_store(&midi->_isMidiOutBusy,
                   false); // Release the lock on failure.
    }
    // On success, the lock will be released in the _handleMidiTransfer()
    // callback after this transfer completes, which might then trigger
    // _processMidiOutQueue() again.
    ESP_LOGI(
        TAG,
        "MIDI OUT transfer submitted (result will be returned via callback).");
  } else {
    atomic_store(
        &midi->_isMidiOutBusy,
        false); // Should not happen if queue was not empty, but as a safeguard.
  }
}

void _releaseDeviceResources(struct UsbMidi *midi) {
  if (!midi->_deviceHandle)
    return;

  ESP_LOGI(TAG, "Releasing MIDI device resources...\n");

  if (midi->_midiOutQueue) {
    xQueueReset(midi->_midiOutQueue);
  }

  for (int i = 0; i < NUM_MIDI_IN_TRANSFERS; ++i) {
    if (midi->_midiInTransfers[i]) {
      usb_host_transfer_free(midi->_midiInTransfers[i]);
      midi->_midiInTransfers[i] = NULL;
    }
  }
  if (midi->_midiOutTransfer) {
    usb_host_transfer_free(midi->_midiOutTransfer);
    midi->_midiOutTransfer = NULL;
  }
  if (midi->_isMidiInterfaceFound) {
    usb_host_interface_release(midi->_clientHandle, midi->_deviceHandle,
                               midi->_midiInterfaceNumber);
  }

  usb_host_device_close(midi->_clientHandle, midi->_deviceHandle);

  midi->_deviceHandle = NULL;
  midi->_isMidiInterfaceFound = false;
  midi->_areEndpointsReady = false;
  atomic_store(&midi->_isMidiOutBusy, false); // Atomically reset the flag.
  ESP_LOGI(TAG, "MIDI device cleaned up.\n");
}