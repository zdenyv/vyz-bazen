#include <zdeny_relay.h>

#define _ZDENY_RELAY_DEFAULT_SLOT_INTERVAL 100

static void _zdeny_relay_gpio_init(zdeny_relay_t *self) {
    twr_gpio_init(self->_channel);
}

static void _zdeny_relay_gpio_on(zdeny_relay_t *self) {
    twr_gpio_set_output(self->_channel, self->_idle_state ? 0 : 1);
}

static void _zdeny_relay_gpio_off(zdeny_relay_t *self) {
    twr_gpio_set_output(self->_channel, self->_idle_state ? 1 : 0);
}

static const zdeny_relay_driver_t _zdeny_relay_driver_gpio = {
    .init = _zdeny_relay_gpio_init,
    .on = _zdeny_relay_gpio_on,
    .off = _zdeny_relay_gpio_off
};

static void _zdeny_relay_task(void *param) {
    zdeny_relay_t *self = param;

    if (self->_pulses_active > 0) {
        self->_driver->off(self);

        self->_pulses_active--;
        self->_selector = 0;
        bool state = twr_gpio_get_output(self->_channel) != self->_idle_state;
        char buffer[20]; // Allocate a buffer of sufficient size
        snprintf(buffer, sizeof(buffer), "relay/%d/state", (int) self->_relay_channel);
        twr_radio_pub_bool(buffer, &state);

        twr_scheduler_plan_current_relative(self->_slot_interval);

        return;
    }

    if (self->_selector == 0) {
        self->_selector = 0x80000000;
    }

    if ((self->_pattern & self->_selector) != 0) {
        self->_driver->on(self);
    } else {
        self->_driver->off(self);
    }

    self->_selector >>= 1;

    if (self->_count != 0) {
        if (--self->_count == 0) {
            self->_pattern = 0;
        }
    }

    twr_scheduler_plan_current_relative(self->_slot_interval);
}

void zdeny_relay_init(zdeny_relay_t *self, zdeny_relay_channel_t channel, twr_gpio_channel_t gpio_channel, bool open_drain_output, int idle_state) {
    memset(self, 0, sizeof(*self));

    self->_relay_channel = channel;
    self->_channel = gpio_channel;

    self->_open_drain_output = open_drain_output;

    self->_idle_state = idle_state;

    self->_driver = &_zdeny_relay_driver_gpio;
    self->_driver->init(self);
    self->_driver->off(self);

    if (self->_open_drain_output) {
        twr_gpio_set_mode(self->_channel, TWR_GPIO_MODE_OUTPUT_OD);
    } else {
        twr_gpio_set_mode(self->_channel, TWR_GPIO_MODE_OUTPUT);
    }

    self->_slot_interval = _ZDENY_RELAY_DEFAULT_SLOT_INTERVAL;

    self->_task_id = twr_scheduler_register(_zdeny_relay_task, self, TWR_TICK_INFINITY);
}

void zdeny_relay_set_slot_interval(zdeny_relay_t *self, twr_tick_t interval) {
    self->_slot_interval = interval;
}

void zdeny_relay_set_mode(zdeny_relay_t *self, zdeny_relay_mode_t mode) {
    uint32_t pattern = self->_pattern;

    switch (mode) {
        case ZDENY_RELAY_MODE_TOGGLE:
        {
            if (pattern == 0) {
                self->_pattern = 0xffffffff;
                self->_count = 0;

                self->_driver->on(self);
            } else if (pattern == 0xffffffff) {
                self->_pattern = 0;
                self->_count = 0;

                if (self->_pulses_active > 0) {
                    self->_pulses_active--;
                }
                self->_driver->off(self);
            }

            return;
        }
        case ZDENY_RELAY_MODE_OFF:
        {
            self->_pattern = 0x00000000;
            self->_count = 0;

            if (self->_pulses_active > 0) {
                self->_pulses_active--;
            }
            self->_driver->off(self);
            return;
        }
        case ZDENY_RELAY_MODE_ON:
        {
            self->_pattern = 0xffffffff;
            self->_count = 0;

            self->_driver->on(self);
            return;
        }
        default:
        {
            return;
        }
    }
}

void zdeny_relay_pulse(zdeny_relay_t *self, twr_tick_t duration) {
    self->_driver->on(self);
    self->_pulses_active++;
    twr_scheduler_plan_from_now(self->_task_id, duration);
}

bool zdeny_relay_is_pulse(zdeny_relay_t *self) {
    return self->_pulses_active > 0;
}
