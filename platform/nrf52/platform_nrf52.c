/*
 * Copyright (c) 2016 Ericsson AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include "boards.h"
#include "sdk_config.h"
#include "app_timer_appsh.h"
#include "app_scheduler.h"
#include "nordic_common.h"
#include "softdevice_handler_appsh.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "ble_6lowpan.h"
#include "mem_manager.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "iot_timer.h"
#include "ipv6_medium.h"
#include "nrf_drv_gpiote.h"
#include "../../platform.h"
#include "../../node.h"

#define APP_TIMER_OP_QUEUE_SIZE             5
#define LWIP_SYS_TICK_MS                    100
#define SCHED_MAX_EVENT_DATA_SIZE           128
#define SCHED_QUEUE_SIZE                    12
#define DEAD_BEEF                           0xDEADBEEF
#define APP_CALVIN_INITTIMER_INTERVAL       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)

APP_TIMER_DEF(m_iot_timer_tick_src_id);
APP_TIMER_DEF(m_calvin_inittimer_id);
eui64_t                                     eui64_local_iid;
static ipv6_medium_instance_t               m_ipv6_medium;
static calvin_gpio_t                        *m_gpios[MAX_GPIOS];

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
	log_error("Error 0x%08lX, Line %ld, File %s", error_code, line_num, p_file_name);
//	NVIC_SystemReset();
	for (;;) {
	}
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
	app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void scheduler_init(void)
{
	APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

static void ip_stack_init(void)
{
	uint32_t err_code;

	err_code = ipv6_medium_eui64_get(m_ipv6_medium.ipv6_medium_instance_id, &eui64_local_iid);
	APP_ERROR_CHECK(err_code);

	err_code = nrf_mem_init();
	APP_ERROR_CHECK(err_code);

	lwip_init();

	err_code = nrf_driver_init();
	APP_ERROR_CHECK(err_code);
}

static void system_timer_callback(iot_timer_time_in_ms_t wall_clock_value)
{
	UNUSED_VARIABLE(wall_clock_value);

	sys_check_timeouts();
}

static void iot_timer_tick_callback(void *p_context)
{
	UNUSED_VARIABLE(p_context);
	uint32_t err_code = iot_timer_update();

	APP_ERROR_CHECK(err_code);
}

static void start_calvin_inittimer(void)
{
	uint32_t err_code;

	err_code = app_timer_start(m_calvin_inittimer_id, APP_CALVIN_INITTIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	log("Started calvin init timer");
}

static void calvin_inittimer_callback(void *p_context)
{
	UNUSED_VARIABLE(p_context);
	log("Executing calvin init timer");
	if (node_start(NULL, "2001:db8::1", 5000) != SUCCESS) {
		log_error("Failed to start node");
		start_calvin_inittimer();
	}
}

static void timers_init(void)
{
	uint32_t err_code;

	APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);

	err_code = app_timer_create(&m_iot_timer_tick_src_id,
								APP_TIMER_MODE_REPEATED,
								iot_timer_tick_callback);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_calvin_inittimer_id,
								APP_TIMER_MODE_SINGLE_SHOT,
								calvin_inittimer_callback);
	APP_ERROR_CHECK(err_code);
}

static void iot_timer_init(void)
{
	uint32_t err_code;

	static const iot_timer_client_t list_of_clients[] = {
		{system_timer_callback,      LWIP_SYS_TICK_MS}
	};

	static const iot_timer_clients_list_t iot_timer_clients = {
		(sizeof(list_of_clients) / sizeof(iot_timer_client_t)),
		&(list_of_clients[0]),
	};

	err_code = iot_timer_client_list_set(&iot_timer_clients);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_start(m_iot_timer_tick_src_id,
							   APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER),
							   NULL);
	APP_ERROR_CHECK(err_code);
}

void nrf_driver_interface_up(void)
{
	log("IPv6 interface up");

	sys_check_timeouts();

	start_calvin_inittimer();
}

void nrf_driver_interface_down(void)
{
	log_debug("IPv6 interface down");
}

static void connectable_mode_enter(void)
{
	uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);

	APP_ERROR_CHECK(err_code);

	log_debug("Physical layer in connectable mode");
}

static void on_ipv6_medium_evt(ipv6_medium_evt_t *p_ipv6_medium_evt)
{
	switch (p_ipv6_medium_evt->ipv6_medium_evt_id) {
	case IPV6_MEDIUM_EVT_CONN_UP:
	{
		log_debug("Physical layer connected");
		break;
	}
	case IPV6_MEDIUM_EVT_CONN_DOWN:
	{
		log_debug("Physical layer disconnected");
		connectable_mode_enter();
		break;
	}
	default:
	{
		break;
	}
	}
}

static void on_ipv6_medium_error(ipv6_medium_error_t *p_ipv6_medium_error)
{
	log_error("Physical layer error");
}

void platform_init(void)
{
	uint32_t err_code;
	uint8_t rnd_seed = 0;
	int i = 0;
	static ipv6_medium_init_params_t ipv6_medium_init_params;
	eui48_t ipv6_medium_eui48;

	app_trace_init();
	scheduler_init();
	timers_init();
	iot_timer_init();

	memset(&ipv6_medium_init_params, 0x00, sizeof(ipv6_medium_init_params));
	ipv6_medium_init_params.ipv6_medium_evt_handler    = on_ipv6_medium_evt;
	ipv6_medium_init_params.ipv6_medium_error_handler  = on_ipv6_medium_error;
	ipv6_medium_init_params.use_scheduler              = true;

	err_code = ipv6_medium_init(&ipv6_medium_init_params, IPV6_MEDIUM_ID_BLE, &m_ipv6_medium);
	APP_ERROR_CHECK(err_code);

	err_code = ipv6_medium_eui48_get(m_ipv6_medium.ipv6_medium_instance_id, &ipv6_medium_eui48);

	ipv6_medium_eui48.identifier[EUI_48_SIZE - 1] = 0x00;

	err_code = ipv6_medium_eui48_set(m_ipv6_medium.ipv6_medium_instance_id, &ipv6_medium_eui48);
	APP_ERROR_CHECK(err_code);

	ip_stack_init();

	connectable_mode_enter();

	for (i = 0; i < MAX_GPIOS; i++)
		m_gpios[i] = NULL;

	do {
		err_code = sd_rand_application_vector_get(&rnd_seed, 1);
	} while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);
	srand(rnd_seed);

	log("Platform initialized");
}

result_t platform_run(const char *ssdp_iface, const char *proxy_iface, const int proxy_port)
{
	uint32_t err_code;

	while (1) {
		app_sched_execute();
		err_code = sd_app_evt_wait();
		APP_ERROR_CHECK(err_code);
	}

	return SUCCESS;
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	// TODO: Create or use NRF SDKs memory pool
	*buffer = malloc(size);
	if (*buffer == NULL) {
		log_error("Failed to allocate '%ld'", (unsigned long)size);
		return FAIL;
	}

	return SUCCESS;
}

void platform_mem_free(void *buffer)
{
	// TODO: Create or use NRF SDKs memory pool
	free(buffer);
}

static void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	int i = 0;

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] != NULL && m_gpios[i]->pin == pin) {
			if (nrf_drv_gpiote_in_is_set(pin))
				m_gpios[i]->value = 1;
			else
				m_gpios[i]->value = 0;
			m_gpios[i]->has_triggered = true;
			node_loop_once();
			return;
		}
	}
}

calvin_gpio_t *platform_create_in_gpio(uint32_t pin, char pull, char edge)
{
	int i = 0;
	ret_code_t err_code;
	nrf_drv_gpiote_in_config_t config;

	memset(&config, 0, sizeof(nrf_drv_gpiote_in_config_t));

	if (pull != 'u' && pull != 'd') {
		log_error("Unsupported pull direction");
		return NULL;
	}

	if (edge != 'r' && edge != 'f' && edge != 'b') {
		log_error("Unsupported edge");
		return NULL;
	}

	if (!nrf_drv_gpiote_is_init()) {
		err_code = nrf_drv_gpiote_init();
		if (err_code != NRF_SUCCESS) {
			log_error("Failed to initialize gpio");
			return NULL;
		}
	}

	if (edge == 'b')
		config.sense = NRF_GPIOTE_POLARITY_TOGGLE;
	else if (edge == 'r')
		config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
	else
		config.sense = NRF_GPIOTE_POLARITY_HITOLO;

	if (pull == 'd')
		config.pull = NRF_GPIO_PIN_PULLDOWN;
	else
		config.pull = NRF_GPIO_PIN_PULLUP;

	err_code = nrf_drv_gpiote_in_init(pin, &config, in_pin_handler);
	if (err_code != NRF_SUCCESS) {
		log_error("Failed to initialize gpio");
		return NULL;
	}

	nrf_drv_gpiote_in_event_enable(pin, true);

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] == NULL) {
			if (platform_mem_alloc((void **)m_gpios[i], sizeof(calvin_gpio_t)) != SUCCESS) {
				log_error("Failed to allocate memory");
				return NULL;
			}
			m_gpios[i]->pin = pin;
			m_gpios[i]->has_triggered = false;
			m_gpios[i]->value = 0;
			m_gpios[i]->direction = GPIO_IN;
			return m_gpios[i];
		}
	}

	return NULL;
}

calvin_gpio_t *platform_create_out_gpio(uint32_t pin)
{
	int i = 0;
	ret_code_t err_code;

	if (!nrf_drv_gpiote_is_init()) {
		err_code = nrf_drv_gpiote_init();
		if (err_code != NRF_SUCCESS) {
			log_error("Failed to initialize gpio");
			return NULL;
		}
	}

	nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);

	err_code = nrf_drv_gpiote_out_init(pin, &out_config);
	if (err_code != NRF_SUCCESS) {
		log_error("Failed to initialize gpio");
		return NULL;
	}

	nrf_drv_gpiote_out_clear(pin);

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] == NULL) {
			if (platform_mem_alloc((void **)m_gpios[i], sizeof(calvin_gpio_t)) != SUCCESS) {
				log_error("Failed to allocate memory");
				return NULL;
			}

			m_gpios[i]->pin = pin;
			m_gpios[i]->direction = GPIO_OUT;
			return m_gpios[i];
		}
	}

	return NULL;
}

void platform_set_gpio(calvin_gpio_t *gpio, uint32_t value)
{
	if (value == 1)
		nrf_drv_gpiote_out_set(gpio->pin);
	else
		nrf_drv_gpiote_out_clear(gpio->pin);
}

void platform_uninit_gpio(calvin_gpio_t *gpio)
{
	int i = 0;

	if (gpio->direction == GPIO_IN)
		nrf_drv_gpiote_in_uninit(gpio->pin);
	else
		nrf_drv_gpiote_out_uninit(gpio->pin);

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] != NULL && m_gpios[i]->pin == gpio->pin) {
			m_gpios[i] = NULL;
			break;
		}
	}

	platform_mem_free(gpio);
}

result_t platform_get_temperature(double *temp)
{
	uint32_t err_code;
	int32_t value;

	err_code = sd_temp_get(&value);

	*temp = value / 4;

	return err_code == NRF_SUCCESS ? SUCCESS : FAIL;
}
