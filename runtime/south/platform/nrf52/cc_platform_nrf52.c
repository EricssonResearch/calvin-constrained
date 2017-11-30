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
#ifdef CC_TLS_ENABLED
#include "nrf_drv_rng.h"
#endif
#include "../cc_platform.h"
#include "../../../north/cc_node.h"
#include "../../../north/coder/cc_coder.h"
#include "../../../north/cc_transport.h"
#include "../../transport/lwip/cc_transport_lwip.h"
#include "../../../../calvinsys/cc_calvinsys.h"

#define APP_TIMER_OP_QUEUE_SIZE							5
#define LWIP_SYS_TIMER_INTERVAL							APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)
#define APP_CALVIN_CONNECT_DELAY						APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) // Delay connect as connect fails directly after interface up

APP_TIMER_DEF(m_lwip_timer_id);
APP_TIMER_DEF(m_calvin_connect_timer_id);
eui64_t                                     eui64_local_iid;
static ipv6_medium_instance_t               m_ipv6_medium;

void cc_platform_nrf_app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)
{
	cc_log_error("Error 0x%08lX, Line %ld, File %s", error_code, line_num, p_file_name);
//	NVIC_SystemReset();
	for (;;) {
	}
}

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
	cc_platform_nrf_app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

static void cc_platform_nrf_connectable_mode_enter(void)
{
	uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);

	APP_ERROR_CHECK(err_code);

	cc_log_debug("Physical layer in connectable mode");
}

static void cc_platform_nrf_on_ipv6_medium_evt(ipv6_medium_evt_t *p_ipv6_medium_evt)
{
	size_t len = 0;
	cc_transport_client_t *transport_client = cc_transport_lwip_get_client();
	cc_transport_lwip_client_t *transport_state = (cc_transport_lwip_client_t *)transport_client->client_state;

	switch (p_ipv6_medium_evt->ipv6_medium_evt_id) {
	case IPV6_MEDIUM_EVT_CONN_UP:
	{
		cc_log("Physical layer connected mac '%s'", p_ipv6_medium_evt->mac);
		strncpy(transport_state->mac, p_ipv6_medium_evt->mac, strlen(p_ipv6_medium_evt->mac));
		break;
	}
	case IPV6_MEDIUM_EVT_CONN_DOWN:
	{
		cc_log("Physical layer disconnected");
		cc_platform_nrf_connectable_mode_enter();
		transport_client->state = CC_TRANSPORT_INTERFACE_DOWN;
		break;
	}
	default:
	{
		break;
	}
	}
}

static void cc_platform_nrf_on_ipv6_medium_error(ipv6_medium_error_t *p_ipv6_medium_error)
{
	cc_log_error("Physical layer error");
}

static void cc_platform_nrf_ip_stack_init(void)
{
	uint32_t err_code;
	static ipv6_medium_init_params_t ipv6_medium_init_params;
	eui48_t ipv6_medium_eui48;

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

	memset(&ipv6_medium_init_params, 0x00, sizeof(ipv6_medium_init_params));
	ipv6_medium_init_params.ipv6_medium_evt_handler    = cc_platform_nrf_on_ipv6_medium_evt;
	ipv6_medium_init_params.ipv6_medium_error_handler  = cc_platform_nrf_on_ipv6_medium_error;
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

static void cc_platform_nrf_lwip_timer_callback(void *p_ctx)
{
	(void) p_ctx;
	sys_check_timeouts();
}

static void cc_platform_nrf_start_calvin_connect_timer(void)
{
	uint32_t err_code;

	err_code = app_timer_start(m_calvin_connect_timer_id, APP_CALVIN_CONNECT_DELAY, NULL);
	APP_ERROR_CHECK(err_code);
}

static void cc_platform_nrf_calvin_connect_callback(void *p_context)
{
	UNUSED_VARIABLE(p_context);
	cc_transport_client_t *transport_client = cc_transport_lwip_get_client();

	transport_client->state = CC_TRANSPORT_INTERFACE_UP;
}


static void cc_platform_nrf_timers_init(void)
{
	uint32_t err_code;

	// Create and start lwip timer
	err_code = app_timer_create(&m_lwip_timer_id, APP_TIMER_MODE_REPEATED, cc_platform_nrf_lwip_timer_callback);
	APP_ERROR_CHECK(err_code);
	err_code = app_timer_start(m_lwip_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	// Create calvin init timer used to start the node when a connection is made
	err_code = app_timer_create(&m_calvin_connect_timer_id,
		APP_TIMER_MODE_SINGLE_SHOT,
		cc_platform_nrf_calvin_connect_callback);
	APP_ERROR_CHECK(err_code);
}

void nrf_driver_interface_up(void)
{
	cc_log("IP interface up");
	cc_platform_nrf_start_calvin_connect_timer();
}

void nrf_driver_interface_down(void)
{
	cc_transport_client_t *transport_client = cc_transport_lwip_get_client();

	transport_client->state = CC_TRANSPORT_INTERFACE_DOWN;
	cc_log("IP interface down");
}

cc_result_t cc_platform_stop(cc_node_t *node)
{
	return CC_SUCCESS;
}

cc_result_t cc_platform_node_started(struct cc_node_t *node)
{
	return CC_SUCCESS;
}

void cc_platform_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\r\n");
	va_end(args);
}

cc_result_t cc_platform_create(cc_node_t *node)
{
	return cc_list_add_n(&node->proxy_uris, "lwip", 4, NULL, 0);
}

static bool cc_platform_temp_can_read(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_platform_temp_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	double temp;
	uint32_t err_code;
	int32_t value;

	err_code = sd_temp_get(&value);

	temp = value / 4;

	*size = cc_coder_sizeof_double(temp);
	if (cc_platform_mem_alloc((void **)data, *size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	cc_coder_encode_double(*data, temp);

	return CC_SUCCESS;
}

static cc_result_t cc_platform_temp_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	obj->can_read = cc_platform_temp_can_read;
	obj->read = cc_platform_temp_read;

	return CC_SUCCESS;
}

cc_result_t cc_platform_add_capabilities(cc_calvinsys_t *calvinsys)
{
	return cc_calvinsys_create_capability(calvinsys, "io.temperature", cc_platform_temp_open, NULL, NULL);
}

void cc_platform_init(void)
{
	uint32_t err_code;
	uint8_t rnd_seed = 0;

	app_trace_init();
	cc_platform_nrf_ip_stack_init();
	err_code = nrf_driver_init();
	APP_ERROR_CHECK(err_code);
	cc_platform_nrf_timers_init();

	cc_platform_nrf_connectable_mode_enter();

	do {
		err_code = sd_rand_application_vector_get(&rnd_seed, 1);
	} while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);
	srand(rnd_seed);

	cc_log("Platform initialized");
}

cc_platform_evt_wait_status_t cc_platform_evt_wait(cc_node_t *node, uint32_t timeout_seconds)
{
	if (sd_app_evt_wait() != ERR_OK) {
		cc_log_error("Failed to wait for event");
		return CC_PLATFORM_EVT_WAIT_FAIL;
	}

	if (node != NULL && node->transport_client != NULL && node->transport_client->state == CC_TRANSPORT_ENABLED) {
		if (cc_transport_lwip_has_data(node->transport_client)) {
			if (cc_transport_handle_data(node, node->transport_client, cc_node_handle_message) != CC_SUCCESS) {
				cc_log_error("Failed to read data from transport");
				node->transport_client->state = CC_TRANSPORT_DISCONNECTED;
				return CC_PLATFORM_EVT_WAIT_FAIL;
			}
			return CC_PLATFORM_EVT_WAIT_DATA_READ;
		}
	}

	return CC_PLATFORM_EVT_WAIT_TIMEOUT;
}

cc_result_t cc_platform_mem_alloc(void **buffer, uint32_t size)
{
	*buffer = malloc(size);
	if (*buffer == NULL) {
		cc_log_error("Failed to allocate '%ld'", (unsigned long)size);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

void cc_platform_mem_free(void *buffer)
{
	free(buffer);
}

uint32_t cc_platform_get_seconds(cc_node_t *node)
{
 // TODO: Implement
 return 0;
}

#ifdef CC_TLS_ENABLED
int cc_platform_random_vector_generate(void *ctx, unsigned char *buffer, size_t size)
{
	uint8_t available;
	uint32_t err_code;
	uint16_t length;

	err_code = nrf_drv_rng_bytes_available(&available);

	cc_log("Requested random numbers 0x%08lx, available 0x%08x",	size,	available);

	if (err_code == NRF_SUCCESS) {
		length = MIN(size, available);
		err_code = nrf_drv_rng_rand(buffer, length);
	}

	return 0;
}
#endif
