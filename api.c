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

#include "api.h"
#include "platform.h"
#include "node.h"
#ifdef MICROPYTHON
#include "libmpy/calvin_mpy_port.h"
#include <unistd.h>
#endif

result_t api_runtime_start(char* name, char* capabilities, char* proxy_uris)
{
    if (name == NULL)
        name = "calvin_constrained";

    if (capabilities == NULL)
        capabilities = "[]";

    platform_init(get_node(), name);

#ifdef MICROPYTHON
    mpy_port_init(MICROPYTHON_HEAP_SIZE);
#endif

    node_run(get_node(), name, proxy_uris);
    return SUCCESS;
}

result_t* api_runtime_init()
{
    node_t* node;
    if (platform_mem_alloc(&node, sizeof(node_t)) != SUCCESS) {
        log_error("Could not allocate memory for node");
    }
    set_node(node);
    node_init(node);
    return SUCCESS;
}

result_t api_runtime_stop()
{
    log_error("Stop not implemented");
    return SUCCESS;
}

result_t api_write_downstream_calvin_payload(node_t* node, char* payload, size_t size)
{
    transport_client_t* tc = node->transport_client;
    api_send_downstream_platform_message(RUNTIME_CALVIN_MSG, tc, payload, size);
}

result_t api_read_upstream(node_t* node, char* buffer, size_t size) {
    int fd = node->platform->upstream_platform_fd[0];
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    int status = select(fd+1, &set, NULL, NULL, NULL);

    if (status > 0) {
        int bytes_read = read(fd, buffer, size);
        if (bytes_read < 0) {
            log_error("Error when reading from pipe");
            return SUCCESS;
        } else {
            return SUCCESS;
        }
    } else if(status < 0) {
        log_error("Error on upstream select");
        return FAIL;
    }
    return SUCCESS;
}

result_t api_send_upstream_calvin_message(char* cmd, transport_client_t* tc, size_t data_size) {
    transport_append_buffer_prefix(tc->tx_buffer.buffer, data_size+3);
    char buffer[BUFFER_SIZE];
    memset(&buffer, 0, BUFFER_SIZE);
    memcpy(buffer, tc->tx_buffer.buffer, 4); // Copy total size to output
    memcpy(buffer+4, cmd, 2); // Copy 2 byte command
    memcpy(buffer+6, tc->tx_buffer.buffer+4, data_size); // Copy payload data
    // Write
    if (write(tc->upstream_fd[1], buffer, data_size+6) < 0) {
        log_error("Could not write to pipe");
    }
    transport_free_tx_buffer(tc);
    return SUCCESS;
}

result_t api_send_downstream_platform_message(char* cmd, transport_client_t* tc, char* data, size_t data_size) {
    // Add command in the first 2 bytes
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memcpy(buffer, cmd, 2);
    memcpy(buffer+2, data, data_size);
    if (write(tc->downstream_fd[1], buffer, data_size+2) < 0) {
        log_error("Could not write to pipe");
    }
    return SUCCESS;
}