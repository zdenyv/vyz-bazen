#include <application.h>
#include <radio.h>
#include <zdeny_relay.h>

#define MAX_SOIL_SENSORS                    5

#define SEKUND                              1000
#define MINUT                               (60 * SEKUND)
#define HODIN                               (60 * MINUT)

#define SERVICE_MODE_INTERVAL               (5 * MINUT)

#define TEMPERATURE_PUB_INTERVAL            (15 * MINUT)
#define TEMPERATURE_PUB_DIFFERENCE          0.5f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (1 * MINUT)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL  (30 * MINUT)
#define TEMPERATURE_DS18B20_PUB_NO_CHANGE_INTEVAL (30 * MINUT)
#define TEMPERATURE_DS18B20_PUB_VALUE_CHANGE 0.2f

#define SENSOR_UPDATE_SERVICE_INTERVAL      (5 * MINUT)
#define SENSOR_UPDATE_NORMAL_INTERVAL       (30 * MINUT)

void radio_pub_state(zdeny_relay_channel_t channel);
void relay_set(uint64_t *id, const char *topic, void *value, void *param);
void relay_pulse(uint64_t *id, const char *topic, void *value, void *param);
void relay_get(uint64_t *id, const char *topic, void *value, void *param);

// subscribe table, format: topic, expect payload type, callback, user param
static const twr_radio_sub_t subs[] = {
    // state/set
    {"relay/1/state/set", TWR_RADIO_SUB_PT_BOOL, relay_set, (void *) RELAY_CHANNEL_1},
    {"relay/2/state/set", TWR_RADIO_SUB_PT_BOOL, relay_set, (void *) RELAY_CHANNEL_2},
    {"relay/3/state/set", TWR_RADIO_SUB_PT_BOOL, relay_set, (void *) RELAY_CHANNEL_3},
    {"relay/4/state/set", TWR_RADIO_SUB_PT_BOOL, relay_set, (void *) RELAY_CHANNEL_4},
    // pulse/set
    {"relay/1/pulse/set", TWR_RADIO_SUB_PT_INT, relay_pulse, (void *) RELAY_CHANNEL_1},
    {"relay/2/pulse/set", TWR_RADIO_SUB_PT_INT, relay_pulse, (void *) RELAY_CHANNEL_2},
    {"relay/3/pulse/set", TWR_RADIO_SUB_PT_INT, relay_pulse, (void *) RELAY_CHANNEL_3},
    {"relay/4/pulse/set", TWR_RADIO_SUB_PT_INT, relay_pulse, (void *) RELAY_CHANNEL_4},
    // state/get
    {"relay/1/state/get", TWR_RADIO_SUB_PT_NULL, relay_get, (void *) RELAY_CHANNEL_1},
    {"relay/2/state/get", TWR_RADIO_SUB_PT_NULL, relay_get, (void *) RELAY_CHANNEL_2},
    {"relay/3/state/get", TWR_RADIO_SUB_PT_NULL, relay_get, (void *) RELAY_CHANNEL_3},
    {"relay/4/state/get", TWR_RADIO_SUB_PT_NULL, relay_get, (void *) RELAY_CHANNEL_4}
};


// LED instance
twr_led_t led;
zdeny_relay_t rele1;
zdeny_relay_t rele2;
zdeny_relay_t rele3;
zdeny_relay_t rele4;
// Button instance
twr_button_t button;
// Thermometer instance
twr_tmp112_t tmp112;
// Soil sensor instance
twr_soil_sensor_t soil_sensor;
// Sensors array
twr_soil_sensor_sensor_t sensors[MAX_SOIL_SENSORS];

twr_module_relay_t relay_0_0;
twr_module_relay_t relay_0_1;

twr_ds18b20_t ds18b20;
struct {
    event_param_t temperature;
    event_param_t temperature_ds18b20;
    event_param_t humidity;
    event_param_t illuminance;
    event_param_t pressure;
} params;


void radio_pub_state(zdeny_relay_channel_t channel) {
    bool state;
    if (channel == RELAY_CHANNEL_1) {
        state = twr_gpio_get_output(TWR_GPIO_P11) == 0;
        twr_radio_pub_bool("relay/1/state", &state);
    } else if (channel == RELAY_CHANNEL_2) {
        state = twr_gpio_get_output(TWR_GPIO_P12) == 0;
        twr_radio_pub_bool("relay/2/state", &state);
    } else if (channel == RELAY_CHANNEL_3) {
        state = twr_gpio_get_output(TWR_GPIO_P13) == 0;
        twr_radio_pub_bool("relay/3/state", &state);
    } else if (channel == RELAY_CHANNEL_4) {
        state = twr_gpio_get_output(TWR_GPIO_P14) == 0;
        twr_radio_pub_bool("relay/4/state", &state);
    }
}

zdeny_relay_t * getRelayAddr(zdeny_relay_channel_t channel) {
    zdeny_relay_t * rele = NULL;
    if (channel == RELAY_CHANNEL_1) {
        rele = &rele1;
    } else if (channel == RELAY_CHANNEL_2) {
        rele = &rele2;
    } else if (channel == RELAY_CHANNEL_3) {
        rele = &rele3;
    } else if (channel == RELAY_CHANNEL_4) {
        rele = &rele4;
    }
    return rele;
}

/*void publish_relay_state_after_pulse(void *param) {
    zdeny_relay_channel_t channel = *(zdeny_relay_channel_t *) param;
    radio_pub_state(channel);
}*/

void relay_set(uint64_t *id, const char *topic, void *value, void *param) {
    twr_log_debug("relay_set on channel %d", (int) param);
    zdeny_relay_channel_t channel = (zdeny_relay_channel_t) param;
    zdeny_relay_set_mode(getRelayAddr(channel), (*(bool *) value)? ZDENY_RELAY_MODE_ON : ZDENY_RELAY_MODE_OFF);
    radio_pub_state(channel);
}

void relay_pulse(uint64_t *id, const char *topic, void *value, void *param) {
    int duration = *(int *) value;
    twr_log_debug("relay_pulse on channel %d with duration %d ms", (int) param, duration);
    zdeny_relay_channel_t channel = (zdeny_relay_channel_t) param;
    zdeny_relay_pulse(getRelayAddr(channel), duration);
    radio_pub_state(channel);
}

void relay_get(uint64_t *id, const char *topic, void *value, void *param) {
    twr_log_debug("relay_get on channel %d", (int) param);
    zdeny_relay_channel_t channel = (zdeny_relay_channel_t) param;
    radio_pub_state(channel);
}

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param) {
    // Counters for button events
    static uint16_t button_click_count = 0;
    static uint16_t button_hold_count = 0;

    if (event == TWR_BUTTON_EVENT_CLICK) {
        // Pulse LED for 100 milliseconds
        twr_led_pulse(&led, 100);

        // Increment press count
        button_click_count++;

        // Publish button message on radio
        twr_radio_pub_push_button(&button_click_count);
        //twr_led_set_mode(&rele1, ZDENY_RELAY_MODE_TOGGLE);
    } else if (event == TWR_BUTTON_EVENT_HOLD) {
        // Pulse LED for 250 milliseconds
        twr_led_pulse(&led, 250);

        // Increment hold count
        button_hold_count++;
        // Publish message on radio
        twr_radio_pub_event_count(TWR_RADIO_PUB_EVENT_HOLD_BUTTON, &button_hold_count);
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param) {
    // Time of next report
    static twr_tick_t tick_report = 0;

    // Last value used for change comparison
    static float last_published_temperature = NAN;

    if (event == TWR_TMP112_EVENT_UPDATE) {
        float temperature;

        if (twr_tmp112_get_temperature_celsius(self, &temperature)) {
            // Implicitly do not publish message on radio
            bool publish = false;

            // Is time up to report temperature?
            if (twr_tick_get() >= tick_report) {
                // Publish message on radio
                publish = true;
            }

            // Is temperature difference from last published value significant?
            if (fabsf(temperature - last_published_temperature) >= TEMPERATURE_PUB_DIFFERENCE) {
                // Publish message on radio
                publish = true;
            }

            if (publish) {
                // Publish temperature message on radio
                twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);

                // Schedule next temperature report
                tick_report = twr_tick_get() + TEMPERATURE_PUB_INTERVAL;

                // Remember last published value
                last_published_temperature = temperature;
            }
        }
    }
}

void twr_radio_node_on_state_get(uint64_t *id, uint8_t state_id) {
    (void) id;
    twr_log_debug("twr_radio_node_on_state_get state_id=%d", state_id);

    switch (state_id) {
        case TWR_RADIO_NODE_STATE_RELAY_MODULE_0:
        {
            twr_module_relay_state_t r_state = twr_module_relay_get_state(&relay_0_0);
            if (r_state != TWR_MODULE_RELAY_STATE_UNKNOWN)
            {
                bool state = r_state == TWR_MODULE_RELAY_STATE_TRUE ? true : false;
                twr_radio_pub_state(TWR_RADIO_PUB_STATE_RELAY_MODULE_0, &state);
            }
            break;
        }
        case TWR_RADIO_NODE_STATE_RELAY_MODULE_1:
        {
            twr_module_relay_state_t r_state = twr_module_relay_get_state(&relay_0_1);
            if (r_state != TWR_MODULE_RELAY_STATE_UNKNOWN)
            {
                bool state = r_state == TWR_MODULE_RELAY_STATE_TRUE ? true : false;
                twr_radio_pub_state(TWR_RADIO_PUB_STATE_RELAY_MODULE_1, &state);
            }

            break;
        }
        default:
        {
            return;
        }
    }
}

void twr_radio_node_on_state_set(uint64_t *id, uint8_t state_id, bool *state) {
    (void) id;
    twr_log_debug("twr_radio_node_on_state_set state_id=%d state=%s", state_id, state? "true" : "false");

    switch (state_id) {
        case TWR_RADIO_NODE_STATE_RELAY_MODULE_0:
        {
            twr_module_relay_set_state(&relay_0_0, *state);
            twr_radio_pub_state(TWR_RADIO_PUB_STATE_RELAY_MODULE_0, state);
            break;
        }
        case TWR_RADIO_NODE_STATE_RELAY_MODULE_1:
        {
            twr_module_relay_set_state(&relay_0_1, *state);
            twr_radio_pub_state(TWR_RADIO_PUB_STATE_RELAY_MODULE_1, state);
            break;
        }
        default:
        {
            return;
        }
    }
}

void soil_sensor_event_handler(twr_soil_sensor_t *self, uint64_t device_address, twr_soil_sensor_event_t event, void *event_param) {
    static char topic[64];
    uint8_t *b = (uint8_t*)&device_address;

    if (event == TWR_SOIL_SENSOR_EVENT_UPDATE) {
        int index = twr_soil_sensor_get_index_by_device_address(self, device_address);

        if (index < 0) {
            return;
        }

        float temperature;

        if (twr_soil_sensor_get_temperature_celsius(self, device_address, &temperature)) {
            snprintf(topic, sizeof(topic), "soil-sensor/%02x%02x%02x%02x%02x%02x%02x%02x/temperature", b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]);
            //snprintf(topic, sizeof(topic), "soil-sensor/%llx/temperature", device_address);

            // Publish temperature message on radio
            twr_radio_pub_float(topic, &temperature);
        }

        uint16_t raw_cap_u16;

        if (twr_soil_sensor_get_cap_raw(self, device_address, &raw_cap_u16)) {
            snprintf(topic, sizeof(topic), "soil-sensor/%02x%02x%02x%02x%02x%02x%02x%02x/raw", b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]);
            //snprintf(topic, sizeof(topic), "soil-sensor/%llx/raw", device_address);

            // Publish raw capacitance value message on radio
            int raw_cap = (int)raw_cap_u16;
            twr_radio_pub_int(topic, &raw_cap);

            // Experimental - send percent moisture value based on sensor calibration
            int moisture;
            twr_soil_sensor_get_moisture(self, device_address, &moisture);
            snprintf(topic, sizeof(topic), "soil-sensor/%llx/moisture", device_address);
            twr_radio_pub_int(topic, &moisture);
        }
    } else if (event == TWR_SOIL_SENSOR_EVENT_ERROR) {
        int error = twr_soil_sensor_get_error(self);
        twr_radio_pub_int("soil-sensor/-/error", &error);
    }
}

void ds18b20_event_handler(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t e, void *p) {
    (void) p;

    float value = NAN;

    if (e == TWR_DS18B20_EVENT_UPDATE) {
        twr_ds18b20_get_temperature_celsius(self, device_address, &value);

        //twr_log_debug("UPDATE %" PRIx64 "(%d) = %f", device_address, device_index, value);

        if ((fabs(value - params.temperature_ds18b20.value) >= TEMPERATURE_DS18B20_PUB_VALUE_CHANGE) || (params.temperature_ds18b20.next_pub < twr_scheduler_get_spin_tick())) {
            static char topic[64];
            snprintf(topic, sizeof(topic), "ext-thermometer/%" PRIx64 "/temperature", device_address);
            twr_radio_pub_float(topic, &value);
            params.temperature_ds18b20.value = value;
            params.temperature_ds18b20.next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_DS18B20_PUB_NO_CHANGE_INTEVAL;
            twr_scheduler_plan_from_now(0, 300);
        }
    }
}

void switch_to_normal_mode_task(void *param) {
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);
    twr_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_NORMAL_INTERVAL);
    twr_ds18b20_set_update_interval(&ds18b20, TEMPERATURE_UPDATE_NORMAL_INTERVAL);
    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void application_init(void) {
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);
    twr_log_debug("Starting...");
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);

    zdeny_relay_init(&rele1, RELAY_CHANNEL_1, TWR_GPIO_P11, true, true);
    zdeny_relay_init(&rele2, RELAY_CHANNEL_2, TWR_GPIO_P12, true, true);
    zdeny_relay_init(&rele3, RELAY_CHANNEL_3, TWR_GPIO_P13, true, true);
    zdeny_relay_init(&rele4, RELAY_CHANNEL_4, TWR_GPIO_P14, true, true);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize thermometer sensor on core module
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);

    // Initialize soil sensor
 /*   twr_soil_sensor_init_multiple(&soil_sensor, sensors, 5);
    twr_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    twr_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_SERVICE_INTERVAL);
*/
    // Initialize relay module(s)
    twr_module_relay_init(&relay_0_0, TWR_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    twr_module_relay_init(&relay_0_1, TWR_MODULE_RELAY_I2C_ADDRESS_ALTERNATE);
    
    // For single sensor you can call twr_ds18b20_init()
/*    twr_ds18b20_init_single(&ds18b20, TWR_DS18B20_RESOLUTION_BITS_12);
    twr_ds18b20_set_event_handler(&ds18b20, ds18b20_event_handler, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
*/
    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_LISTENING);
    twr_radio_set_subs((twr_radio_sub_t *) subs, sizeof(subs)/sizeof(twr_radio_sub_t));

    twr_radio_pairing_request("zdeny-bazen", "2.3");
    
    twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_MODE_INTERVAL);

    twr_led_pulse(&led, 2000);
}

// Application task function (optional) which is called peridically if scheduled
/*void application_task(void) {
    radio_pub_state(RELAY_CHANNEL_1);
    radio_pub_state(RELAY_CHANNEL_2);
    radio_pub_state(RELAY_CHANNEL_3);
    radio_pub_state(RELAY_CHANNEL_4);
    twr_scheduler_plan_current_from_now(10 * SEKUND);
}*/