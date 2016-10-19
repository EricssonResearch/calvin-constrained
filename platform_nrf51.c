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
#include "boards.h"
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
#include "lwip/init.h"
#include "lwip/timers.h"
#include "nrf_drv_gpiote.h"
#include "platform.h"
#include "node.h"
#include "transport.h"

#define DEVICE_NAME                         "calvin-constrained"
#define APP_TIMER_PRESCALER                 NRF51_DRIVER_TIMER_PRESCALER
#define LWIP_SYS_TIMER_INTERVAL             APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)
#define APP_CALVIN_INITTIMER_INTERVAL       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
#define SCHED_MAX_EVENT_DATA_SIZE           128
#define SCHED_QUEUE_SIZE                    12
#define APP_TIMER_MAX_TIMERS                3
#define APP_TIMER_OP_QUEUE_SIZE             3
#define APP_ADV_TIMEOUT                     0
#define APP_ADV_ADV_INTERVAL                MSEC_TO_UNITS(100, UNIT_0_625_MS)
#define DEAD_BEEF                           0xDEADBEEF

eui64_t                                     eui64_local_iid;
static ble_gap_adv_params_t                 m_adv_params;
static app_timer_id_t                       m_sys_timer_id;
static app_timer_id_t                       m_calvin_inittimer_id;
static char                                 m_mac[20]; // MAC address of connected peer
static calvin_gpio_t                        *m_gpios[MAX_GPIOS];

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
	log_debug("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s", error_code, line_num, p_file_name);

	for (;;) {
	}
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
	app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

void start_calvin_inittimer(void)
{
	uint32_t err_code;

	err_code = app_timer_start(m_calvin_inittimer_id, APP_CALVIN_INITTIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);
}

static void advertising_init(void)
{
	uint32_t                err_code;
	ble_advdata_t           advdata;
	uint8_t                 flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
	ble_gap_conn_sec_mode_t sec_mode;
	ble_gap_addr_t          my_addr;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode,
										  (const uint8_t *)DEVICE_NAME,
										  strlen(DEVICE_NAME));
	APP_ERROR_CHECK(err_code);

	err_code = sd_ble_gap_address_get(&my_addr);
	APP_ERROR_CHECK(err_code);

	my_addr.addr[5]   = 0x00;
	my_addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;

	err_code = sd_ble_gap_address_set(&my_addr);
	APP_ERROR_CHECK(err_code);

	IPV6_EUI64_CREATE_FROM_EUI48(eui64_local_iid.identifier,
								 my_addr.addr,
								 my_addr.addr_type);

	ble_uuid_t adv_uuids[] = {
		{BLE_UUID_IPSP_SERVICE,         BLE_UUID_TYPE_BLE}
	};

	memset(&advdata, 0, sizeof(advdata));

	advdata.name_type               = BLE_ADVDATA_FULL_NAME;
	advdata.flags                   = flags;
	advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
	advdata.uuids_complete.p_uuids  = adv_uuids;

	err_code = ble_advdata_set(&advdata, NULL);
	APP_ERROR_CHECK(err_code);

	memset(&m_adv_params, 0, sizeof(m_adv_params));

	m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
	m_adv_params.p_peer_addr = NULL;
	m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
	m_adv_params.interval    = APP_ADV_ADV_INTERVAL;
	m_adv_params.timeout     = APP_ADV_TIMEOUT;
}

static void advertising_start(void)
{
	uint32_t err_code;

	err_code = sd_ble_gap_adv_start(&m_adv_params);
	APP_ERROR_CHECK(err_code);
}

static void on_ble_evt(ble_evt_t *p_ble_evt)
{
	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		sprintf(m_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr[5],
			p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr[4],
			p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr[3],
			p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr[2],
			p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr[1],
			p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr[0]);
		log_debug("Connected to: %s", m_mac);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		log_debug("BLE disconnected");
		advertising_start();
		break;
	default:
		break;
	}
}

static void ble_evt_dispatch(ble_evt_t *p_ble_evt)
{
	ble_ipsp_evt_handler(p_ble_evt);
	on_ble_evt(p_ble_evt);
}

static void ble_stack_init(void)
{
	uint32_t err_code;

	SOFTDEVICE_HANDLER_APPSH_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, true);

	err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
	APP_ERROR_CHECK(err_code);
}

static void scheduler_init(void)
{
	APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

static void ip_stack_init(void)
{
	uint32_t err_code;

	err_code = nrf51_sdk_mem_init();
	APP_ERROR_CHECK(err_code);

	lwip_init();

	err_code = nrf51_driver_init();
	APP_ERROR_CHECK(err_code);
}

static void system_timer_callback(void *p_context)
{
	UNUSED_VARIABLE(p_context);
	sys_check_timeouts();
}

static void calvin_inittimer_callback(void *p_context)
{
	UNUSED_VARIABLE(p_context);
	if (start_node(m_mac) != SUCCESS)
		log_error("Failed to start node");
}

static void timers_init(void)
{
	uint32_t err_code;

	APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

	err_code = app_timer_create(&m_sys_timer_id,
								APP_TIMER_MODE_REPEATED,
								system_timer_callback);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_calvin_inittimer_id,
								APP_TIMER_MODE_SINGLE_SHOT,
								calvin_inittimer_callback);
	APP_ERROR_CHECK(err_code);
}

void nrf51_driver_interface_up(void)
{
	uint32_t err_code;

	log_debug("IPv6 interface up");

	sys_check_timeouts();

	err_code = app_timer_start(m_sys_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	start_calvin_inittimer();
}

void nrf51_driver_interface_down(void)
{
	uint32_t err_code;

	log_debug("IPv6 interface down");

	err_code = app_timer_stop(m_sys_timer_id);
	APP_ERROR_CHECK(err_code);
}

void platform_init(void)
{
	uint32_t err_code;
	uint8_t rnd_seed;
	int i = 0;

	app_trace_init();
	timers_init();
	ble_stack_init();
	advertising_init();
	ip_stack_init();
	scheduler_init();

	for (i = 0; i < MAX_GPIOS; i++)
		m_gpios[i] = NULL;
	do {
		err_code = sd_rand_application_vector_get(&rnd_seed, 1);
	} while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);
	srand(rnd_seed);

	log_debug("Init done");
}

void platform_run(void)
{
	uint32_t err_code;

	advertising_start();

	while (1) {
		app_sched_execute();
		err_code = sd_app_evt_wait();
		if (err_code != NRF_SUCCESS)
			log_error("sd_app_evt_wait failed");
	}
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
			loop_once();
			return;
		}
	}
}

calvin_gpio_t *create_in_gpio(uint32_t pin, char pull, char edge)
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
			m_gpios[i] = (calvin_gpio_t *)malloc(sizeof(calvin_gpio_t));
			if (m_gpios[i] == NULL) {
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

calvin_gpio_t *create_out_gpio(uint32_t pin)
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
			m_gpios[i] = (calvin_gpio_t *)malloc(sizeof(calvin_gpio_t));
			if (m_gpios[i] == NULL) {
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

void set_gpio(calvin_gpio_t *gpio, uint32_t value)
{
	if (value == 1)
		nrf_drv_gpiote_out_set(gpio->pin);
	else
		nrf_drv_gpiote_out_clear(gpio->pin);
}

void uninit_gpio(calvin_gpio_t *gpio)
{
	int i = 0;

	for (i = 0; i < MAX_GPIOS; i++) {
		if (m_gpios[i] != NULL && m_gpios[i]->pin == gpio->pin) {
			m_gpios[i] = NULL;
			break;
		}
	}

	if (gpio->direction == GPIO_IN)
		nrf_drv_gpiote_in_uninit(gpio->pin);
	else
		nrf_drv_gpiote_out_uninit(gpio->pin);

	free(gpio);
}

result_t get_temperature(double *temp)
{
	uint32_t err_code;
	int32_t value;

	err_code = sd_temp_get(&value);

	*temp = value / 4;

	return err_code == NRF_SUCCESS ? SUCCESS : FAIL;
}
