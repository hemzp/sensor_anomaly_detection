#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_attr.h"

static const char *TAG = "rpm";

//meeting notes: tweak the config values below, trial and error.

// ---------------------------------------------------------------------------
// Configuration — tweak these for your setup
// ---------------------------------------------------------------------------
#define RPM_GPIO_PIN          GPIO_NUM_6
#define PULSES_PER_REV        1
#define MIN_VALID_PERIOD_US   100000U    // 100 ms debounce
#define TIMEOUT_MS            90000U     // 90 s -> shaft stopped

// ---------------------------------------------------------------------------
// Shared state (ISR writes, main reads) — volatile + spinlock-protected
// ---------------------------------------------------------------------------
static volatile int64_t  s_last_edge_us    = 0;
static volatile uint32_t s_period_us       = 0;
static volatile uint32_t s_pulse_count     = 0;
static volatile bool     s_first_edge_seen = false;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// ---------------------------------------------------------------------------
// ISR — runs on every falling edge. IRAM_ATTR keeps it safe during flash ops.
// ---------------------------------------------------------------------------
static void IRAM_ATTR rpm_isr(void *arg) {
    int64_t now = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&s_mux); // claude gave some spinlock safety here, essentially only allows one core to access the vars at a time.


    //indentifying and ignoring the first pulse.
    if (!s_first_edge_seen) {
        // First edge: nothing to subtract from, just record it.
        s_last_edge_us    = now;
        s_first_edge_seen = true;
        s_pulse_count++;
        portEXIT_CRITICAL_ISR(&s_mux);
        return;
    }

    int64_t delta = now - s_last_edge_us; //time between current pulse and last pulse, in microseconds

    // Debounce: reject edges closer than physically possible.
    if (delta < (int64_t)MIN_VALID_PERIOD_US) {
        portEXIT_CRITICAL_ISR(&s_mux);
        return;
    }

    s_period_us    = (uint32_t)delta;
    s_last_edge_us = now;
    s_pulse_count++;

    portEXIT_CRITICAL_ISR(&s_mux);
}

// ---------------------------------------------------------------------------
// RPM reader — called from main loop at whatever rate you want
// ---------------------------------------------------------------------------
static uint32_t compute_rpm(void) {
    // Atomic snapshot of shared state
    uint32_t period_us;
    int64_t  last_edge_us;
    bool     first_seen;

    portENTER_CRITICAL(&s_mux);
    period_us    = s_period_us;
    last_edge_us = s_last_edge_us;
    first_seen   = s_first_edge_seen;
    portEXIT_CRITICAL(&s_mux);

    // Return 0 if we have no valid reading yet
    if (!first_seen || period_us == 0) return 0;

    // Timeout: shaft stopped
    int64_t now = esp_timer_get_time();
    if ((now - last_edge_us) > ((int64_t)TIMEOUT_MS * 1000)) {
        return 0;
    }

    // RPM = 60 s/min * 1,000,000 us/s / (period_us * PPR)
    uint64_t numerator = 60ULL * 1000000ULL;
    uint64_t denom     = (uint64_t)period_us * PULSES_PER_REV;
    if (denom == 0) return 0;
    return (uint32_t)(numerator / denom);
}

// ---------------------------------------------------------------------------
// One-time setup — configures the pin and registers the ISR
// ---------------------------------------------------------------------------
static void rpm_setup(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RPM_GPIO_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Install ISR service (ignore "already installed" error)
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(RPM_GPIO_PIN, rpm_isr, NULL));

    ESP_LOGI(TAG, "ready on GPIO %d", RPM_GPIO_PIN);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
void app_main(void) {
    rpm_setup();

    while (1) {
        // Snapshot pulse count for diagnostics
        uint32_t pulses;
        portENTER_CRITICAL(&s_mux);
        pulses = s_pulse_count;
        portEXIT_CRITICAL(&s_mux);

        uint32_t rpm = compute_rpm();

        ESP_LOGI(TAG, "RPM = %lu  (pulses seen: %lu)",
                 (unsigned long)rpm,
                 (unsigned long)pulses);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}