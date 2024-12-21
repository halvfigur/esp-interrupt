#include <stdio.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"

// GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, and GPIO11 cannot be used as ESP32 interrupt pins.
#define INPUT_PIN GPIO_NUM_18
#define OUTPUT_PIN GPIO_NUM_17
#define ESP_INTR_FLAG_DEFAULT 0

#define STACK_DEPTH 4096
#define INTERRUPT_TASK_PRIO 10

TaskHandle_t isr_task_handle = NULL;

// IRAM_ATTR Forces code into IRAM instead of flash
void IRAM_ATTR input_isr_handler(void *arg) {
  xTaskResumeFromISR(isr_task_handle);
}

void interrupt_task(void *arg) {
  bool output_status = false;

  for (;;) {
    printf("Interrupted\n");
    output_status = !output_status;
    printf("Setting output %s\n", output_status ? "high" : "low");
    vTaskSuspend(isr_task_handle);
    if (gpio_set_level(OUTPUT_PIN, (uint32_t)output_status) != ESP_OK) {
      printf("gpio_set_level(): failed\n");
    }
  }
}

esp_err_t gpio_setup(void) {

  // Input pin configuration
  gpio_config_t input_config = {};
  input_config.pin_bit_mask = (1ULL << INPUT_PIN);
  input_config  .mode = GPIO_MODE_INPUT;
  input_config.pull_up_en = GPIO_PULLUP_ENABLE;
  input_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  input_config.intr_type = GPIO_INTR_POSEDGE;

  // Output pin configuration
  gpio_config_t output_config = {};
  output_config.pin_bit_mask = (1ULL << OUTPUT_PIN);
  output_config.mode = GPIO_MODE_OUTPUT;
  output_config.pull_up_en = GPIO_PULLUP_DISABLE;
  output_config.pull_down_en = GPIO_PULLDOWN_DISABLE;

  esp_err_t err;

  printf("Configuring input pin\n");
  if ((err = gpio_config(&input_config)) != ESP_OK) {
    printf("gpio_config(): failed for input\n");
    return err;
  }

  printf("Configuring outptu pin\n");
  if ((err = gpio_config(&output_config)) != ESP_OK) {
    printf("gpio_config(): failed for output\n");
    return err;
  }

  if ((err = gpio_dump_io_configuration(stdout, SOC_GPIO_VALID_GPIO_MASK)) != ESP_OK) {
    printf("gpio_dump_io_configuration(): failed\n");
  }

  return ESP_OK;
}

void app_main(void) {
  esp_err_t err;

  if ((err = gpio_setup()) != ESP_OK) {
    printf("gpio_setup(): failed\n");
    return;
  }

  // Install the GPIO driver's ETS_GPIO_INTR_SOURCE ISR handler service, which allows per-pin GPIO interrupt handlers.
  if ((err = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT)) != ESP_OK) {
    printf("gpio_install_isr_service(): failed\n");
    return;
  }

  // Add ISR handler for the corresponding GPIO pin.
  if ((err = gpio_isr_handler_add(INPUT_PIN, input_isr_handler, NULL)) != ESP_OK) {
    printf("gpio_isr_handler_add(): failed\n");
    return;
  }

  xTaskCreate(interrupt_task, "interruptTask", STACK_DEPTH, NULL, INTERRUPT_TASK_PRIO, &isr_task_handle);
}
