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
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "transport.h"
#include "platform.h"
#include "node.h"

#define RECV_BUFFER_SIZE 2048

transport_client_t *client_connect(const char *address, int port)
{
    struct sockaddr_in server;
    transport_client_t *client = NULL;

    client = (transport_client_t*)malloc(sizeof(transport_client_t));
    if (client == NULL) {
        log_error("Failed to allocate memory");
        return NULL;
    }

    client->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->fd < 0) {
        log_error("Failed to create socket");
        free(client);
        return NULL;
    }

    client->state = TRANSPORT_CONNECTED;
    client->buffer = NULL;
    client->buffer_pos = 0;
    client->msg_size = 0;

    server.sin_addr.s_addr = inet_addr(address);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (connect(client->fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        log_error("Failed to connect socket");
        free(client);
        return NULL;
    }

    log_debug("Connected to %s:%d", address, port);

    return client;
}

result_t client_send(transport_client_t *client, char *data, size_t len)
{
    result_t result = SUCCESS;

    data[0] = (len & 0xFF000000);
    data[1] = (len & 0x00FF0000);
    data[2] = (len & 0x0000FF00) / 0x000000FF;
    data[3] = (len & 0x000000FF);

    if (send(client->fd, data, len + 4, 0) < 0) {
        result = FAIL;
        log_error("Failed to send data");
    }

    free(data);

    return result;
}

result_t wait_for_data(transport_client_t **client, uint32_t timeout)
{
    result_t result = SUCCESS;
    fd_set fd_set;
    int max_fd = 0, status = 0, read_pos = 0, msg_size = 0;
    char buffer[RECV_BUFFER_SIZE];
    struct timeval tv = {timeout, 0};

    FD_ZERO(&fd_set);

    // stdin
    FD_SET(0, &fd_set);

    // proxy socket
    if ((*client)->state != TRANSPORT_DISCONNECTED) {
        FD_SET((*client)->fd, &fd_set);
        if ((*client)->fd > max_fd) {
            max_fd = (*client)->fd;
        }
    }

    if (select(max_fd + 1, &fd_set, NULL, NULL, &tv) < 0) {
        log_error("ERROR on select");
        return FAIL;
    }

    bzero(buffer, RECV_BUFFER_SIZE);

    if (FD_ISSET(0, &fd_set)) {
        stop_node();
    }

    if (FD_ISSET((*client)->fd, &fd_set)) {
        if ((status = recv((*client)->fd, buffer, RECV_BUFFER_SIZE, 0)) == 0) {
            // TODO: disconnect tunnels using this connection
            (*client)->state = TRANSPORT_DISCONNECTED;
            log_debug("Connection closed");
        } else {
            while (read_pos < status) {
                if ((*client)->buffer == NULL) {
                    msg_size = get_message_len(buffer + read_pos);
                    read_pos += 4; // Skip header
                    if (msg_size <= status - read_pos) {
                        handle_data(buffer +read_pos, msg_size, *client);
                        read_pos += msg_size;
                    } else {
                        (*client)->buffer = malloc(msg_size);
                        if ((*client)->buffer == NULL) {
                            log_error("Failed to allocate memory");
                            return FAIL;
                        }
                        memcpy((*client)->buffer, buffer + read_pos, status - read_pos);
                        (*client)->msg_size = msg_size;
                        (*client)->buffer_pos = status - read_pos;
                        return SUCCESS;
                    }
                } else {
                    msg_size = (*client)->msg_size;
                    if (msg_size - (*client)->buffer_pos <= status - read_pos) {
                        memcpy((*client)->buffer + (*client)->buffer_pos, buffer + read_pos, msg_size - (*client)->buffer_pos);
                        handle_data((*client)->buffer, msg_size, *client);                        
                        free((*client)->buffer);
                        (*client)->buffer = NULL;
                        read_pos += msg_size - (*client)->buffer_pos;
                        (*client)->buffer = 0;
                        (*client)->msg_size = 0;
                        (*client)->buffer_pos = 0;                        
                    } else {
                        memcpy((*client)->buffer + (*client)->buffer_pos, buffer + read_pos, status - read_pos);
                        (*client)->buffer_pos += status - read_pos;
                        return SUCCESS;
                    }
                }
            }
        }
    }

    return result;
}

void free_client(transport_client_t *client)
{
    free(client);
}