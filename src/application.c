#include <application.h>
#include <radio.h>

#define MAX_SOIL_SENSORS                    5

#define SEKUND                              1000
#define MINUT                               (60 * SEKUND)
#define HODIN                               (60 * MINUT)

#define SERVICE_MODE_INTERVAL               (5 * MINUT)
#define BATTERY_UPDATE_INTERVAL             (10 * HODIN)

#define TEMPERATURE_PUB_INTERVAL            (15 * MINUT)
#define TEMPERATURE_PUB_DIFFERENCE          0.5f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (1 * MINUT)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL  (30 * MINUT)
#define TEMPERATURE_DS18B20_PUB_NO_CHANGE_INTEVAL (30 * MINUT)
#define TEMPERATURE_DS18B20_PUB_VALUE_CHANGE 0.2f

#define SENSOR_UPDATE_SERVICE_INTERVAL      (5 * MINUT)
#define SENSOR_UPDATE_NORMAL_INTERVAL       (30 * MINUT)

// LED instance
twr_led_t led;
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
bool stav = false;

twr_ds18b20_t ds18b20;
struct {
    event_param_t temperature;
    event_param_t temperature_ds18b20;
    event_param_t humidity;
    event_param_t illuminance;
    event_param_t pressure;
} params;

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
        stav = !stav;
        twr_module_relay_set_state(&relay_0_0, stav);
        twr_radio_pub_state(TWR_RADIO_PUB_STATE_RELAY_MODULE_0, &stav);
    }
    else if (event == TWR_BUTTON_EVENT_HOLD)
    {
        // Pulse LED for 250 milliseconds
        twr_led_pulse(&led, 250);

        // Increment hold count
        button_hold_count++;
        // Publish message on radio
        twr_radio_pub_event_count(TWR_RADIO_PUB_EVENT_HOLD_BUTTON, &button_hold_count);
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
        }
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
    //twr_log_debug(sprintf("twr_radio_node_on_state_get state_id=%d", state_id));

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
    (void)pointer;   // VyZ disable unused warning
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
            snprintf(topic, sizeof(topic), "soil-sensor/%02x%02x%02x%02x%02x%02x%02x%02x/temperature", b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0]);
            //snprintf(topic, sizeof(topic), "soil-sensor/%llx/temperature", device_address);

            // Publish temperature message on radio
            twr_radio_pub_float(topic, &temperature);
        }

        uint16_t raw_cap_u16;

        if (twr_soil_sensor_get_cap_raw(self, device_address, &raw_cap_u16))
        {
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

            // ZDENY timeout in ms for receiving relay command
            twr_radio_set_rx_timeout_for_sleeping_node(1000);
        }
    }
    else if (event == TWR_SOIL_SENSOR_EVENT_ERROR)
    {
        int error = twr_soil_sensor_get_error(self);
        twr_radio_pub_int("soil-sensor/-/error", &error);
    }
}

void ds18b20_event_handler(twr_ds18b20_t *self, uint64_t device_address, twr_ds18b20_event_t e, void *p)
{
    (void) p;

    float value = NAN;

    if (e == TWR_DS18B20_EVENT_UPDATE)
    {
        twr_ds18b20_get_temperature_celsius(self, device_address, &value);

        //twr_log_debug("UPDATE %" PRIx64 "(%d) = %f", device_address, device_index, value);

        if ((fabs(value - params.temperature_ds18b20.value) >= TEMPERATURE_DS18B20_PUB_VALUE_CHANGE) || (params.temperature_ds18b20.next_pub < twr_scheduler_get_spin_tick()))
        {
            static char topic[64];
            snprintf(topic, sizeof(topic), "ext-thermometer/%" PRIx64 "/temperature", device_address);
            twr_radio_pub_float(topic, &value);
            params.temperature_ds18b20.value = value;
            params.temperature_ds18b20.next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_DS18B20_PUB_NO_CHANGE_INTEVAL;
            twr_scheduler_plan_from_now(0, 300);
        }
    }
}

void switch_to_normal_mode_task(void *param)
{
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);
    twr_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_NORMAL_INTERVAL);
    twr_ds18b20_set_update_interval(&ds18b20, TEMPERATURE_UPDATE_NORMAL_INTERVAL);
    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize thermometer sensor on core module
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);

    // Initialize soil sensor
    twr_soil_sensor_init_multiple(&soil_sensor, sensors, 5);
    twr_soil_sensor_set_event_handler(&soil_sensor, soil_sensor_event_handler, NULL);
    twr_soil_sensor_set_update_interval(&soil_sensor, SENSOR_UPDATE_SERVICE_INTERVAL);

    // Initialize relay module(s)
    twr_module_relay_init(&relay_0_0, TWR_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    twr_module_relay_init(&relay_0_1, TWR_MODULE_RELAY_I2C_ADDRESS_ALTERNATE);
    //twr_log_debug("init rele ok");

    // For single sensor you can call twr_ds18b20_init()
    twr_ds18b20_init_single(&ds18b20, TWR_DS18B20_RESOLUTION_BITS_12);
    twr_ds18b20_set_event_handler(&ds18b20, ds18b20_event_handler, NULL);
    twr_ds18b20_set_update_interval(&ds18b20, TEMPERATURE_UPDATE_SERVICE_INTERVAL);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_pairing_request("zdeny-bazen-s-baterkama", "2.2");

    twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_MODE_INTERVAL);

    twr_led_pulse(&led, 2000);
}
