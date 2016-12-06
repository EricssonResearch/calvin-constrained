/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 */

/** @cond To make doxygen skip this file */

/** @file
 *  This header contains defines with respect to the IPv6 medium that are specific to
 *  the BLE implementation and the application use case.
 * @{
 */

#ifndef IPV6_MEDIUM_BLE_CONFIG_H__
#define IPV6_MEDIUM_BLE_CONFIG_H__

#include "app_util.h"

#define DEVICE_NAME                      "calvin_constrained"                 /**< Device name used in BLE undirected advertisement. */
#define IS_SRVC_CHANGED_CHARACT_PRESENT  0

#define COMPANY_IDENTIFIER               0x0059                               /**< Company identifier for Nordic Semiconductor ASA as per www.bluetooth.org. */

#define APP_TIMER_PRESCALER              31                                   /**< Value of the RTC1 PRESCALER register. */
#define APP_ADV_ADV_INTERVAL             MSEC_TO_UNITS(333, UNIT_0_625_MS)    /**< The advertising interval. This value can vary between 100ms to 10.24s). */
#define APP_ADV_TIMEOUT                  0                                    /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */

#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(7.5, UNIT_1_25_MS)     /**< Minimum connection interval (7.5 ms) */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(30, UNIT_1_25_MS)      /**< Maximum connection interval (30 ms). */
#define SLAVE_LATENCY                    6                                    /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(430, UNIT_10_MS)       /**< Connection supervisory timeout (430 ms). */

#endif // IPV6_MEDIUM_BLE_CONFIG_H__
