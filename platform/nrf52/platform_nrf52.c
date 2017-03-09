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
#include "app_timer.h"
#include "boards.h"
#include "sdk_config.h"
#include "nordic_common.h"
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
#include "../../transport.h"
#include "../../transport_lwip.h"

#define APP_TIMER_OP_QUEUE_SIZE							5
#define LWIP_SYS_TIMER_INTERVAL							APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)
#define APP_CALVIN_CONNECT_DELAY						APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) // Delay connect as connect fails directly after interface up

APP_TIMER_DEF(m_lwip_timer_id);
APP_TIMER_DEF(m_calvin_connect_timer_id);
eui64_t                                     eui64_local_iid;
static ipv6_medium_instance_t               m_ipv6_medium;
static calvinsys_io_giohandler_t 						m_io_gpiohandler;

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
	log_error("Error 0x%08lX, Line %ld, File %s", error_code, line_num, p_file_name);
//	NVIC_SystemReset();
	for (;;) {
	}
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
	app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

static void connectable_mode_enter(void)
{
	uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);

	APP_ERROR_CHECK(err_code);

	log_debug("Physical layer in connectable mode");
}

static void on_ipv6_medium_evt(ipv6_medium_evt_t *p_ipv6_medium_evt)
{
	size_t len = 0;
	transport_client_t *transport_client = transport_get_client();
	transport_lwip_client_t *transport_state = (transport_lwip_client_t *)transport_client->client_state;

	switch (p_ipv6_medium_evt->ipv6_medium_evt_id) {
	case IPV6_MEDIUM_EVT_CONN_UP:
	{
		log("Physical layer connected mac '%s'", p_ipv6_medium_evt->mac);
		strncpy(transport_state->mac, p_ipv6_medium_evt->mac, strlen(p_ipv6_medium_evt->mac));
		break;
	}
	case IPV6_MEDIUM_EVT_CONN_DOWN:
	{
		log("Physical layer disconnected");
		connectable_mode_enter();
		transport_client->state = TRANSPORT_INTERFACE_DOWN;
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

static void ip_stack_init(void)
{
	uint32_t err_code;
	static ipv6_medium_init_params_t ipv6_medium_init_params;
	eui48_t ipv6_medium_eui48;

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

	memset(&ipv6_medium_init_params, 0x00, sizeof(ipv6_medium_init_params));
	ipv6_medium_init_params.ipv6_medium_evt_handler    = on_ipv6_medium_evt;
	ipv6_medium_init_params.ipv6_medium_error_handler  = on_ipv6_medium_error;
	ipv6_medium_init_params.use_scheduler              = false;

	err_code = ipv6_medium_init(&ipv6_medium_init_params, IPV6_MEDIUM_ID_BLE, &m_ipv6_medium);
	APP_ERROR_CHECK(err_code);

	err_code = ipv6_medium_eui48_get(m_ipv6_medium.ipv6_medium_instance_id, &ipv6_medium_eui48);

	ipv6_medium_eui48.identifier[EUI_48_SIZE - 1] = 0x00;

	err_code = ipv6_medium_eui48_set(m_ipv6_medium.ipv6_medium_instance_id, &ipv6_medium_eui48);
	APP_ERROR_CHECK(err_code);

	err_code = ipv6_medium_eui64_get(m_ipv6_medium.ipv6_medium_instance_id, &eui64_local_iid);
	APP_ERROR_CHECK(err_code);

	err_code = nrf_mem_init();
	APP_ERROR_CHECK(err_code);

	lwip_init();
}

static void lwip_timer_callback(void *p_ctx)
{
	(void) p_ctx;
	sys_check_timeouts();
}

static void start_calvin_connect_timer(void)
{
	uint32_t err_code;

  err_code = app_timer_start(m_calvin_connect_timer_id, APP_CALVIN_CONNECT_DELAY, NULL);
  APP_ERROR_CHECK(err_code);
}

static void calvin_connect_callback(void *p_context)
{
  UNUSED_VARIABLE(p_context);
	transport_client_t *transport_client = transport_get_client();

	transport_client->state = TRANSPORT_INTERFACE_UP;
}


static void timers_init(void)
{
	uint32_t err_code;

	// Create and start lwip timer
	err_code = app_timer_create(&m_lwip_timer_id,
															APP_TIMER_MODE_REPEATED,
															lwip_timer_callback);
	APP_ERROR_CHECK(err_code);
	err_code = app_timer_start(m_lwip_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	// Create calvin init timer used to start the node when a connection is made
	err_code = app_timer_create(&m_calvin_connect_timer_id,
		APP_TIMER_MODE_SINGLE_SHOT,
		calvin_connect_callback);
	APP_ERROR_CHECK(err_code);
}

void nrf_driver_interface_up(void)
{
	log("IP interface up");
	start_calvin_connect_timer();
}

void nrf_driver_interface_down(void)
{
	transport_client_t *transport_client = transport_get_client();

	transport_client->state = TRANSPORT_INTERFACE_DOWN;
	log("IP interface down");
}

static void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	int i = 0;

	for (i = 0; i < MAX_INGPIOS; i++) {
		if (m_io_gpiohandler.ingpios[i] != NULL && m_io_gpiohandler.ingpios[i]->pin == pin) {
			m_io_gpiohandler.ingpios[i]->has_triggered = true;
			m_io_gpiohandler.ingpios[i]->value = nrf_drv_gpiote_in_is_set(pin) ? 1 : 0;
			return;
		}
	}
}

void platform_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\r\n");
	va_end(args);
}

result_t platform_create(node_t* node)
{
	char *uri = NULL;

	if (platform_mem_alloc((void **)&uri, 5) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	strncpy(uri, "lwip", 4);
	uri[4] = '\0';
	node->proxy_uris[0] = uri;

	return SUCCESS;
}

static calvin_ingpio_t *platform_init_in_gpio(calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, char pull, char edge)
{
	nrf_drv_gpiote_in_config_t config;
	int i = 0;

	memset(&config, 0, sizeof(nrf_drv_gpiote_in_config_t));

	if (pull != 'u' && pull != 'd') {
		log_error("Unsupported pull direction");
		return NULL;
	}

	if (edge != 'r' && edge != 'f' && edge != 'b') {
		log_error("Unsupported edge");
		return NULL;
	}

	for (i = 0; i < MAX_INGPIOS; i++) {
		if (gpiohandler->ingpios[i] == NULL) {
			if (!nrf_drv_gpiote_is_init()) {
				if (nrf_drv_gpiote_init() != NRF_SUCCESS) {
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

			if (nrf_drv_gpiote_in_init(pin, &config, in_pin_handler) != NRF_SUCCESS) {
				log_error("Failed to initialize gpio");
				return NULL;
			}

			nrf_drv_gpiote_in_event_enable(pin, true);

			if (platform_mem_alloc((void **)&gpiohandler->ingpios[i], sizeof(calvin_ingpio_t)) != SUCCESS) {
				log_error("Failed to allocate memory");
				nrf_drv_gpiote_out_uninit(pin);
				return NULL;
			}

			gpiohandler->ingpios[i]->pin = pin;
			gpiohandler->ingpios[i]->has_triggered = false;
			gpiohandler->ingpios[i]->pull = pull;
			gpiohandler->ingpios[i]->edge = edge;
			return gpiohandler->ingpios[i];
		}
	}

	return NULL;
}

static result_t platform_init_out_gpio(uint32_t pin)
{
	if (!nrf_drv_gpiote_is_init()) {
		if (nrf_drv_gpiote_init() != NRF_SUCCESS) {
			log_error("Failed to initialize gpio");
			return FAIL;
		}
	}

	nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);

	if (nrf_drv_gpiote_out_init(pin, &out_config) != NRF_SUCCESS) {
		log_error("Failed to initialize gpio");
		return FAIL;
	}

	log("Initialized gpio '%ld' as output", (unsigned long)pin);

	return SUCCESS;
}

static void platform_set_gpio(uint32_t pin, uint32_t value)
{
	if (value == 1)
		nrf_drv_gpiote_out_set(pin);
	else
		nrf_drv_gpiote_out_clear(pin);
}

static void platform_uninit_gpio(calvinsys_io_giohandler_t *gpiohandler, uint32_t pin, gpio_direction_t direction)
{
	int i = 0;

	if (direction == GPIO_IN)
		nrf_drv_gpiote_in_uninit(pin);
	else
		nrf_drv_gpiote_out_uninit(pin);

	for (i = 0; i < MAX_INGPIOS; i++) {
		if (m_io_gpiohandler.ingpios[i] != NULL && m_io_gpiohandler.ingpios[i]->pin == pin) {
			platform_mem_free(m_io_gpiohandler.ingpios[i]);
			m_io_gpiohandler.ingpios[i] = NULL;
			break;
		}
	}
}

static result_t platform_get_temperature(double *temp)
{
	uint32_t err_code;
	int32_t value;

	err_code = sd_temp_get(&value);

	*temp = value / 4;

	return err_code == NRF_SUCCESS ? SUCCESS : FAIL;
}

static result_t platform_create_sensors_environmental(node_t *node)
{
	char name[] = "calvinsys.sensors.environmental";
	calvinsys_sensors_environmental_t *sensors_env = NULL;

	if (platform_mem_alloc((void **)&sensors_env, sizeof(calvinsys_sensors_environmental_t)) != SUCCESS) {
		log_error("Failed to allocate memory");
		return FAIL;
	}

	sensors_env->get_temperature = platform_get_temperature;

 	return list_add_n(&node->calvinsys, name, strlen(name), sensors_env, sizeof(calvinsys_sensors_environmental_t));
}

static result_t platform_create_io_gpiohandler(node_t *node)
{
	char name[] = "calvinsys.io.gpiohandler";
	int i = 0;

	m_io_gpiohandler.init_in_gpio = platform_init_in_gpio;
	m_io_gpiohandler.init_out_gpio = platform_init_out_gpio;
	m_io_gpiohandler.set_gpio = platform_set_gpio;
	m_io_gpiohandler.uninit_gpio = platform_uninit_gpio;

	for (i = 0; i < MAX_INGPIOS; i++)
		m_io_gpiohandler.ingpios[i] = NULL;

	return list_add_n(&node->calvinsys, name, strlen(name), &m_io_gpiohandler, sizeof(calvinsys_io_giohandler_t));
}

result_t platform_create_calvinsys(node_t *node)
{
	if (platform_create_sensors_environmental(node) != SUCCESS)
		return FAIL;

	if (platform_create_io_gpiohandler(node) != SUCCESS)
		return FAIL;

	return SUCCESS;
}

void platform_init(void)
{
	uint32_t err_code;
	uint8_t rnd_seed = 0;

	app_trace_init();
	ip_stack_init();
	err_code = nrf_driver_init();
	APP_ERROR_CHECK(err_code);
	timers_init();

	connectable_mode_enter();

	do {
		err_code = sd_rand_application_vector_get(&rnd_seed, 1);
	} while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);
	srand(rnd_seed);

	log("Platform initialized");
}

void platform_evt_wait(node_t *node, struct timeval *timeout)
{
	if (sd_app_evt_wait() != ERR_OK)
		log_error("sd_app_evt_wait failed");
}

result_t platform_mem_alloc(void **buffer, uint32_t size)
{
	// TODO: If fragmentation is a problem create a pool used for allocations
	*buffer = malloc(size);
	if (*buffer == NULL) {
		log_error("Failed to allocate '%ld'", (unsigned long)size);
		return FAIL;
	}

	return SUCCESS;
}

void platform_mem_free(void *buffer)
{
	free(buffer);
}
