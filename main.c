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
#include "platform.h"
#include "node.h"
#ifndef NRF51
#include <stdlib.h>
#include <string.h>
#endif

int main(void)
{
    char name[20];

	platform_init();

    sprintf(name, "rt%d", rand());

    if (create_node(1, 1, name) == SUCCESS) {
#ifdef NRF51
    	// Node is started in platform_nrf51.c when interface is up to get mac address
        // of the connected peer. 
        platform_run();
#else
        if (start_node("0.0.0.0") == SUCCESS) {
        	node_run();
        } else {
        	log_error("Failed to start node");
        }
#endif
    } else {
    	log_error("Failed to create node");
    }

    return 0;
}