#include <application.h>
#include <radio.h>
#include <twr.h>

#define MAX_SOIL_SENSORS                    5

#define SERVICE_MODE_INTERVAL               (15 * 60 * 1000)

#define TEMPERATURE_PUB_INTERVAL            (15 * 60 * 1000)
#define TEMPERATURE_PUB_DIFFERENCE          0.2f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (1 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL  (10 * 1000)

#define SENSOR_UPDATE_SERVICE_INTERVAL      (15 * 1000)
#define SENSOR_UPDATE_NORMAL_INTERVAL       (5 * 60 * 1000)

// LED instance
twr_led_t led;
bool led_state = false;
// Button instance
twr_button_t button;
// Thermometer instance
twr_tmp112_t tmp112;
// Soil sensor instance
twr_soil_sensor_t soil_sensor;
// Sensors array
twr_soil_sensor_sensor_t sensors[MAX_SOIL_SENSORS];

static twr_module_relay_t relay_0_0;
static twr_module_relay_t relay_0_1;

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    // Counters for button events
    static uint16_t button_click_count = 0;
    static uint16_t button_hold_count = 0;

    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        // Pulse LED for 100 milliseconds
        twr_led_pulse(&led, 100);

        // Increment press count
        button_click_count++;

        // Publish button message on radio
        twr_radio_pub_push_button(&button_click_count);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        // Pulse LED for 250 milliseconds
        twr_led_pulse(&led, 300);

        // Increment hold count
        button_hold_count++;
        // Publish message on radio
        twr_radio_pub_event_count(TWR_RADIO_PUB_EVENT_HOLD_BUTTON, &button_hold_count);
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    // Time of next report
    static twr_tick_t tick_report = 0;

    // Last value used for change comparison
    static float last_published_temperature = NAN;

    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        float temperature;

        if (twr_tmp112_get_temperature_celsius(self, &temperature))
        {
            // Implicitly do not publish message on radio
            bool publish = false;

            // Is time up to report temperature?
            if (twr_tick_get() >= tick_report)
            {
                // Publish message on radio
                publish = true;
            }

            // Is temperature difference from last published value significant?
            if (fabsf(temperature - last_published_temperature) >= TEMPERATURE_PUB_DIFFERENCE)
            {
                // Publish message on radio
                publish = true;
            }

            if (publish)
            {
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

void twr_radio_node_on_state_get(uint64_t *id, uint8_t state_id)
{
    (void) id;

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
        case TWR_RADIO_NODE_STATE_POWER_MODULE_RELAY:
        {
            bool state = twr_module_power_relay_get_state();
            twr_radio_pub_state(TWR_RADIO_PUB_STATE_POWER_MODULE_RELAY, &state);
            break;
        }
        default:
        {
            return;
        }
    }
}

void twr_radio_node_on_state_set(uint64_t *id, uint8_t state_id, bool *state)
{
    (void) id;

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
        case TWR_RADIO_NODE_STATE_POWER_MODULE_RELAY:
        {
            twr_module_power_relay_set_state(*state);
            twr_radio_pub_state(TWR_RADIO_PUB_STATE_POWER_MODULE_RELAY, state);
            break;
        }
        default:
        {
            return;
        }
    }
}

void twr_radio_pub_on_buffer(uint64_t *peer_device_address, uint8_t *buffer, size_t length)
{
    (void) peer_device_address;
    if (length < (1 + sizeof(uint64_t)))
    {
        return;
    }

    uint64_t device_address;
    uint8_t *pointer = buffer + sizeof(uint64_t) + 1;

    memcpy(&device_address, buffer + 1, sizeof(device_address));

    if (device_address != twr_radio_get_my_id())
    {
        return;
    }

    switch (buffer[0]) {
        case RADIO_RELAY_0_PULSE_SET:
        case RADIO_RELAY_1_PULSE_SET:
        {
            if (length != (1 + sizeof(uint64_t) + 1 + 4))
            {
                return;
            }
            uint32_t duration; // Duration is 4 byte long in a radio packet, but 8 bytes as a twr_relay_pulse parameter.
            memcpy(&duration, &buffer[sizeof(uint64_t) + 2], sizeof(uint32_t));
            twr_module_relay_pulse(buffer[0] == RADIO_RELAY_0_PULSE_SET ? &relay_0_0 : &relay_0_1, buffer[sizeof(uint64_t) + 1], (twr_tick_t)duration);
            break;
        }
        default:
        {
            break;
        }
    }
}

void soil_sensor_event_handler(twr_soil_sensor_t *self, uint64_t device_address, twr_soil_sensor_event_t event, void *event_param)
{
    static char topic[64];
    uint8_t *b = (uint8_t*)&device_address;

    if (event == TWR_SOIL_SENSOR_EVENT_UPDATE)
    {
        int index = twr_soil_sensor_get_index_by_device_address(self, device_address);

        if (index < 0)
        {
            return;
        }

        float temperature;

        if (twr_soil_sensor_get_temperature_celsius(self, device_address, &temperature))
        {
            snprintf(topic, sizeof(topic), "soil-senzor/%02x%02x%02x%02x%02x%02x%02x%02x/temperature", b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]);
            //snprintf(topic, sizeof(topic), "soil-senzor/%llx/temperature", device_address);

            // Publish temperature message on radio
            twr_radio_pub_float(topic, &temperature);
        }

        uint16_t raw_cap_u16;

        if (twr_soil_sensor_get_cap_raw(self, device_address, &raw_cap_u16))
        {
            snprintf(topic, sizeof(topic), "soil-senzor/%02x%02x%02x%02x%02x%02x%02x%02x/raw", b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]);
            //snprintf(topic, sizeof(topic), "soil-senzor/%llx/raw", device_address);

            // Publish raw capacitance value message on radio
            int raw_cap = (int)raw_cap_u16;
            twr_radio_pub_int(topic, &raw_cap);

            /*
            // Experimental - send percent moisture value based on sensor calibration
            int moisture;
            twr_soil_sensor_get_moisture(self, device_address, &moisture);
            snprintf(topic, sizeof(topic), "soil-senzor/%llx/moisture", device_address);
            twr_radio_pub_int(topic, &moisture);
            */
        }
    }
    else if (event == TWR_SOIL_SENSOR_EVENT_ERROR)
    {
        int error = twr_soil_sensor_get_error(self);
        twr_radio_pub_int("soil-senzor/-/error", &error);
    }
}

void switch_to_normal_mode_task(void *param)
{
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);
    twr_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_NORMAL_INTERVAL);
    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);
    //twr_log_debug("log 1");
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);
    //twr_log_debug("log 2");

    // Initialize thermometer sensor on core module
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
    //twr_log_debug("log 3");

    // Initialize soil sensor
    twr_soil_sensor_init_multiple(&soil_sensor, sensors, 5);
    twr_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    twr_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_SERVICE_INTERVAL);
    //twr_log_debug("log 4");

    // Initialize power module
    twr_module_power_init();
    //twr_log_debug("log 5");

    // Initialize relay module(s)
    twr_module_relay_init(&relay_0_0, TWR_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    twr_module_relay_init(&relay_0_1, TWR_MODULE_RELAY_I2C_ADDRESS_ALTERNATE);
    //twr_log_debug("log 6");

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_LISTENING);
    twr_radio_pairing_request("vyz-bazen", VERSION);
    //twr_log_debug("log 7");

    twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_MODE_INTERVAL);

    twr_led_pulse(&led, 2000);
}
