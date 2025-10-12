#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "example";

#define BLINK_GPIO 48
#define CONFIG_BLINK_PERIOD 1000

static uint8_t s_led_state = 0;

static led_strip_handle_t led_strip;

static void blink_led(void) {
  /* If the addressable LED is enabled */
  if (s_led_state > 0) {
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    switch (s_led_state) {
    case 1:
      led_strip_set_pixel(led_strip, 0, 1, 0, 0);
      break;
    case 2:
      led_strip_set_pixel(led_strip, 0, 0, 1, 0);
      break;
    case 3:
      led_strip_set_pixel(led_strip, 0, 0, 0, 1);
      break;
    }
    /* Refresh the strip to send data */
    led_strip_refresh(led_strip);
  } else {
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
  }

  if (s_led_state == 3) {
    s_led_state = 0;
  } else {
    s_led_state = s_led_state + 1;
  }
}

static void configure_led(void) {
  ESP_LOGI(TAG, "Example configured to blink addressable LED!");
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = BLINK_GPIO,
      .max_leds = 1, // at least one LED on board
  };
  // ????
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
      .flags.with_dma = false,
  };
  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  /* Set all LED off to clear all pixels */
  led_strip_clear(led_strip);
}

void app_main(void) {

  configure_led();

  while (1) {

    ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
    blink_led();
    vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
  }
}