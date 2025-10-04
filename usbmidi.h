/*
 * ESP32 USB MIDI Host Library (ESP32 USB MIDI Omocha)
 * Copyright (c) 2025 ndenki
 * https://github.com/enudenki/esp32-usb-host-midi-library.git
 */

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <usb/usb_host.h>

#ifndef __USB_HOST_MIDI
#define __USB_HOST_MIDI

#define USB_MIDI_DEBUG 1
#define MIDI_OUT_QUEUE_SIZE 128

#define NUM_MIDI_IN_TRANSFERS 2
#define MAX_CLIENT_EVENT_MESSAGES 5
#define USB_EVENT_POLL_TICKS 1
#define USB_AUDIO_SUBCLASS_MIDI_STREAMING 3

typedef enum {
  MIDI_CIN_NOTE_OFF = 0x08,
  MIDI_CIN_NOTE_ON = 0x09,
  MIDI_CIN_CONTROL_CHANGE = 0x0B,
  MIDI_CIN_PROGRAM_CHANGE = 0x0C,
} MidiCin;

typedef void (*MidiMessageCallback)(const uint8_t message[4]);
typedef void (*DeviceCallback)(void);

struct UsbMidi {
  usb_host_client_handle_t _clientHandle;
  usb_device_handle_t _deviceHandle;
  usb_transfer_t *_midiOutTransfer;
  usb_transfer_t *_midiInTransfers[NUM_MIDI_IN_TRANSFERS];
  QueueHandle_t _midiOutQueue;
  uint8_t _midiInterfaceNumber;
  bool _isMidiInterfaceFound;
  bool _areEndpointsReady;
  atomic_bool _isMidiOutBusy;
  MidiMessageCallback _midiMessageCallback;
  DeviceCallback _deviceConnectedCallback;
  DeviceCallback _deviceDisconnectedCallback;
};

void UsbMidiInit(struct UsbMidi *midi);

void usb_midi_destroy(struct UsbMidi *midi);

void usb_midi_begin(struct UsbMidi *midi);
void UsbMidi_update(struct UsbMidi *midi);

void UsbMidi_onMidiMessage(struct UsbMidi *midi, MidiMessageCallback callback);

bool UsbMidi_sendMidiMessage(struct UsbMidi *midi, const uint8_t *message,
                             uint8_t size);
bool UsbMidi_noteOn(struct UsbMidi *midi, uint8_t channel, uint8_t note,
                    uint8_t velocity);
bool UsbMidi_noteOff(struct UsbMidi *midi, uint8_t channel, uint8_t note,
                     uint8_t velocity);
bool UsbMidi_controlChange(struct UsbMidi *midi, uint8_t channel,
                           uint8_t controller, uint8_t value);
bool UsbMidi_programChange(struct UsbMidi *midi, uint8_t channel,
                           uint8_t program);

void UsbMidi_onDeviceConnected(struct UsbMidi *midi, DeviceCallback callback);
void UsbMidi_onDeviceDisconnected(struct UsbMidi *midi,
                                  DeviceCallback callback);

#endif