#include <twr_gpio.h>
#include <twr_scheduler.h>

typedef enum {
    RELAY_CHANNEL_1 = 1,
    RELAY_CHANNEL_2 = 2,
    RELAY_CHANNEL_3 = 3,
    RELAY_CHANNEL_4 = 4
} zdeny_relay_channel_t;

typedef enum {
    //! @brief RELAY toggles between on/off state (this has no effect while processing alternating patterns)
    ZDENY_RELAY_MODE_TOGGLE = 0,

    //! @brief RELAY has steady off state
    ZDENY_RELAY_MODE_OFF = 1,

    //! @brief RELAY has steady on state
    ZDENY_RELAY_MODE_ON = 2,

} zdeny_relay_mode_t;

typedef struct zdeny_relay_t zdeny_relay_t;

//! @brief RELAY driver interface

typedef struct {
    //! @brief Callback for initialization
    void (*init)(zdeny_relay_t *self);

    //! @brief Callback for setting RELAY on
    void (*on)(zdeny_relay_t *self);

    //! @brief Callback for setting RELAY off
    void (*off)(zdeny_relay_t *self);

} zdeny_relay_driver_t;

struct zdeny_relay_t {
    zdeny_relay_channel_t _relay_channel;
    twr_gpio_channel_t _channel;
    const zdeny_relay_driver_t *_driver;
    bool _open_drain_output;
    int _idle_state;
    twr_tick_t _slot_interval;
    uint32_t _pattern;
    uint32_t _selector;
    int _count;
    int _pulses_active;
    twr_scheduler_task_id_t _task_id;
};

//! @brief Initialize RELAY
//! @param[in] self Instance
//! @param[in] channel relay channel on relay board
//! @param[in] gpio_channel GPIO channel RELAY is connected to
//! @param[in] open_drain_output Determines if RELAY is driven by open-drain output
//! @param[in] idle_state GPIO pin idle state (when RELAY is supposed to be off)

void zdeny_relay_init(zdeny_relay_t *self, zdeny_relay_channel_t channel, twr_gpio_channel_t gpio_channel, bool open_drain_output, int idle_state);

//! @brief Set slot interval for pattern processing
//! @param[in] self Instance
//! @param[in] interval Desired slot interval in ticks

void zdeny_relay_set_slot_interval(zdeny_relay_t *self, twr_tick_t interval);

//! @brief Set RELAY mode
//! @param[in] self Instance
//! @param[in] mode Desired RELAY mode

void zdeny_relay_set_mode(zdeny_relay_t *self, zdeny_relay_mode_t mode);

//! @brief Turn on RELAY for the specified duration of time
//! @param[in] self Instance
//! @param[in] duration Duration for which RELAY will be turned on

void zdeny_relay_pulse(zdeny_relay_t *self, twr_tick_t duration);

//! @brief Check if there is ongoing RELAY pulse
//! @param[in] self Instance
//! @return true If pulse is now active
//! @return false If pulse is now inactive

bool zdeny_relay_is_pulse(zdeny_relay_t *self);
