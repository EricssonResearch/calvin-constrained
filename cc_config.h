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
#ifndef CC_CONFIG_H
#define CC_CONFIG_H

#ifdef CC_CONFIGFILE_H
#include CC_CONFIGFILE_H
#endif

// Default config values, use CC_CONFIGFILE to override

// Debug logs
#ifndef CC_DEBUG
#define CC_DEBUG (0)
#endif

// Enable parsing of runtime attributes from command line arguments
#ifndef CC_USE_GETOPT
#define CC_USE_GETOPT (0)
#endif

// Array of capabilities {name, open_function}
#ifndef CC_CAPABILITIES
#define CC_CAPABILITIES
#endif

// Array of C actors {actor_type, setup_function}
#ifndef CC_C_ACTORS
#define CC_C_ACTORS
#endif

// Array of transport clients {schema, create_function}
#ifndef CC_TRANSPORTS
#define CC_TRANSPORTS {}
#endif

// Enable Python actors
#ifndef CC_USE_PYTHON
#define CC_USE_PYTHON (0)
#endif

// Python default config
#if CC_USE_PYTHON
#ifndef CC_PYTHON_FLOATS
#define CC_PYTHON_FLOATS (1)
#endif
#define CC_USE_STORAGE (1)
#ifndef CC_PYTHON_HEAP_SIZE
#define CC_PYTHON_HEAP_SIZE (25 * 1024)
#endif
#ifndef CC_PYTHON_STACK_SIZE
#define CC_PYTHON_STACK_SIZE (8 * 1024)
#endif
#ifndef CC_ACTOR_MODULES_DIR
#define CC_ACTOR_MODULES_DIR "mpys/"
#endif
#endif

// Time, in seconds, to wait before reconnecting transport
#ifndef CC_RECONNECT_TIMEOUT
#define CC_RECONNECT_TIMEOUT (5)
#endif

// Default timeout, in seconds, waiting for an event
#ifndef CC_INACTIVITY_TIMEOUT
#define CC_INACTIVITY_TIMEOUT (2)
#endif

// Enable platform sleep
#ifndef CC_USE_SLEEP
#define CC_USE_SLEEP (0)
#endif

// Default sleep time, in seconds, if no timers are active
#if CC_USE_SLEEP
#ifndef CC_SLEEP_TIME
#define CC_SLEEP_TIME (60)
#endif
#endif

// Enable checkpointing, write state on state change
#ifndef CC_USE_CHECKPOINTING
#define CC_USE_CHECKPOINTING (0)
#endif

#if CC_USE_CHECKPOINTING
#define CC_USE_STORAGE (1)
#endif

// Enable storage, used to serialize node state
#ifndef CC_USE_STORAGE
#define CC_USE_STORAGE (0)
#endif

// Required storage config
#if CC_USE_STORAGE
#ifndef CC_STATE_FILE
#define CC_STATE_FILE "calvin.msgpack"
#endif
#endif

// WIFI AP config
#ifndef CC_USE_WIFI_AP
#define CC_USE_WIFI_AP (0)
#endif

#if CC_USE_WIFI_AP
#ifndef CC_WIFI_CONFIG_FILE
#define CC_WIFI_CONFIG_FILE	"wifi.msgpack"
#endif
#ifndef CC_WIFI_AP_SSID
#define CC_WIFI_AP_SSID "constrained"
#endif
#ifndef CC_WIFI_AP_PSK
#define CC_WIFI_AP_PSK "constrained"
#endif
#endif

#endif /* CC_CONFIG_H */
