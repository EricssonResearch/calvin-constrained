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
#include "app_button.h"
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
#include "platform.h"
#include "node.h"
#include "transport.h"

#define DEVICE_NAME                         "calvin-constrained"
#define APP_TIMER_PRESCALER                 NRF51_DRIVER_TIMER_PRESCALER
#define LWIP_SYS_TIMER_INTERVAL             APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)
#define APP_CALVIN_INITTIMER_INTERVAL       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
#define APP_CALVIN_TIMER_INTERVAL           APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER)
#define SCHED_MAX_EVENT_DATA_SIZE           128
#define SCHED_QUEUE_SIZE                    12
#define ADVERTISING_LED                     BSP_LED_0_MASK
#define CONNECTED_LED                       BSP_LED_1_MASK
#define TCP_CONNECTED_LED                   BSP_LED_2_MASK
#define DISPLAY_LED_0                       BSP_LED_0_MASK
#define DISPLAY_LED_1                       BSP_LED_1_MASK
#define DISPLAY_LED_2                       BSP_LED_2_MASK
#define DISPLAY_LED_3                       BSP_LED_3_MASK
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                            BSP_LED_2_MASK | BSP_LED_3_MASK)
#define APP_TIMER_MAX_TIMERS                3
#define APP_TIMER_OP_QUEUE_SIZE             3
#define APP_ADV_TIMEOUT                     0
#define APP_ADV_ADV_INTERVAL                MSEC_TO_UNITS(100, UNIT_0_625_MS)
#define DEAD_BEEF                           0xDEADBEEF

eui64_t                                     eui64_local_iid;
static ble_gap_adv_params_t                 m_adv_params;
static app_timer_id_t                       m_sys_timer_id;
static app_timer_id_t                       m_calvin_inittimer_id;
static app_timer_id_t                       m_calvin_timer_id;
static char                                 m_mac[20]; // MAC address of connected peer

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    log_debug("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s", error_code, line_num, p_file_name);

    LEDS_ON(ALL_APP_LED);
    for(;;) {
    }
}

void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void leds_init(void)
{
    LEDS_CONFIGURE(ALL_APP_LED);
    LEDS_OFF(ALL_APP_LED);
}

void start_calvin_inittimer()
{
    uint32_t err_code = app_timer_start(m_calvin_inittimer_id, APP_CALVIN_INITTIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}
void start_calvin_timer()
{
    uint32_t err_code = app_timer_start(m_calvin_timer_id, APP_CALVIN_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

void stop_calvin_timer()
{
    uint32_t err_code = app_timer_stop(m_calvin_timer_id);
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

    ble_uuid_t adv_uuids[] =
    {
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

    LEDS_ON(ADVERTISING_LED);
}

static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
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

static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
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
    uint32_t err_code = nrf51_sdk_mem_init();
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
    if (start_node(m_mac) == SUCCESS) {
        LEDS_OFF(CONNECTED_LED);
        LEDS_ON(TCP_CONNECTED_LED);
        start_calvin_timer();
    } else {
        log_error("Failed to start node");
    }
}

static void calvin_timer_callback(void *p_context)
{
    UNUSED_VARIABLE(p_context);
    loop_once();
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

    err_code = app_timer_create(&m_calvin_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                calvin_timer_callback);
    APP_ERROR_CHECK(err_code);
}

void nrf51_driver_interface_up(void)
{
    uint32_t err_code;

    log_debug("IPv6 interface up");

    sys_check_timeouts();

    err_code = app_timer_start(m_sys_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);

    LEDS_OFF(ADVERTISING_LED);
    LEDS_ON(CONNECTED_LED);

    start_calvin_inittimer();
}

void nrf51_driver_interface_down(void)
{
    uint32_t err_code;

    log_debug("IPv6 interface down");

    stop_calvin_timer();

    err_code = app_timer_stop(m_sys_timer_id);
    APP_ERROR_CHECK(err_code);

    LEDS_OFF((DISPLAY_LED_0 | DISPLAY_LED_1 | DISPLAY_LED_2 | DISPLAY_LED_3));
    LEDS_ON(ADVERTISING_LED);
}

void platform_init()
{
    uint32_t err_code;
    uint8_t rnd_seed;

    app_trace_init();
    leds_init();
    timers_init();
    ble_stack_init();
    advertising_init();
    ip_stack_init();
    scheduler_init();

    do
    {
        err_code = sd_rand_application_vector_get(&rnd_seed, 1);
    } while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);

    srand(rnd_seed);

    log_debug("Init done");
}

void platform_run()
{
    uint32_t err_code;

    advertising_start();

    while (1) {
        app_sched_execute();
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

calvin_timer_t *create_recurring_timer(double interval)
{
    calvin_timer_t *calvin_timer = NULL;

    calvin_timer = (calvin_timer_t*)malloc(sizeof(calvin_timer_t));
    if (calvin_timer == NULL) {
        log_error("Failed to allocate memory");
        return NULL;
    }

    calvin_timer->interval = interval;
    if (app_timer_cnt_get(&calvin_timer->last_triggered) != NRF_SUCCESS) {
        log_error("Failed to get time");
    }

    return calvin_timer;
}

void stop_timer(calvin_timer_t *timer)
{
    if (timer != NULL) {
        free(timer);
    }
}

bool check_timer(calvin_timer_t *timer)
{
    uint32_t now, diff;

    if (app_timer_cnt_get(&now) != NRF_SUCCESS) {
        log_error("Failed to get time");
        return false;
    }

    if (app_timer_cnt_diff_compute(now, timer->last_triggered, &diff) != NRF_SUCCESS) {
        log_error("Failed to compute time diff");
        return false;
    }

    if (diff >= APP_TIMER_TICKS(timer->interval * 1000, APP_TIMER_PRESCALER)) {
        timer->last_triggered = now;
        return true;
    }

    return false;
}