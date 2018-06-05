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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <coap/coap.h>
#include "cc_libcoap_client.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/north/cc_actor_store.h"
#include "runtime/south/platform/cc_platform.h"
#include "jsmn/jsmn.h"

cc_result_t cc_actor_coap_setup(struct cc_actor_type_t *type);

typedef struct cc_coap_state_t {
	int fd;
	coap_uri_t uri;
	uint16_t message_id;
} cc_coap_state_t;

typedef struct cc_coap_init_args_t {
	char uri[100];
} cc_coap_init_args_t;

static cc_result_t cc_coap_send_pdu(struct cc_calvinsys_obj_t *obj, bool start)
{
	cc_result_t result = CC_SUCCESS;
	cc_coap_state_t *state = (cc_coap_state_t *)obj->state;
	coap_pdu_t *request = NULL;
	unsigned char _buf[40];
	unsigned char *buf = _buf;
	size_t buflen = 40;
	int res;

	request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET, state->message_id++, COAP_DEFAULT_MTU);
	if (request == NULL) {
		cc_log_error("Failed to create PDU");
		return CC_FAIL;
	}

	if (coap_pdu_encode_header(request, COAP_PROTO_UDP)) {
		if (start)
			coap_add_option(request, COAP_OPTION_OBSERVE, coap_encode_var_bytes(buf, COAP_OBSERVE_ESTABLISH), buf);
		else
			coap_add_option(request, COAP_OPTION_OBSERVE, coap_encode_var_bytes(buf, COAP_OBSERVE_CANCEL), buf);
		coap_add_option(request, COAP_OPTION_URI_PORT, coap_encode_var_bytes(buf, state->uri.port), buf);
		if (state->uri.path.length) {
			res = coap_split_path(state->uri.path.s, state->uri.path.length, buf, &buflen);
			while (res--) {
				coap_add_option(request, COAP_OPTION_URI_PATH, coap_opt_length(buf), coap_opt_value(buf));
				buf += coap_opt_size(buf);
			}
		}
		coap_add_option(request, COAP_OPTION_CONTENT_FORMAT, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);
		coap_add_option(request, COAP_OPTION_ACCEPT, 0, buf);
		if (send(state->fd, request->token - request->hdr_size, request->used_size + request->hdr_size, 0) <= 0) {
			cc_log_error("Failed to send data");
			result = CC_FAIL;
		}
	} else {
		cc_log_error("Failed to encode header");
		result = CC_FAIL;
	}

	coap_delete_pdu(request);

	return result;
}

static bool cc_coap_can_read(struct cc_calvinsys_obj_t *obj)
{
	return FD_ISSET(((cc_coap_state_t *)obj->state)->fd, &obj->capability->calvinsys->node->fds) > 0;
}

static cc_result_t cc_coap_send_ack(int fd, coap_pdu_t *request)
{
	coap_pdu_t *response = NULL;
	cc_result_t result = CC_SUCCESS;

	response = coap_pdu_init(COAP_MESSAGE_ACK, 0, request->tid, 0);
	if (response == NULL) {
		cc_log_error("Failed to init pdu");
		return CC_FAIL;
	}

	if (coap_pdu_encode_header(response, COAP_PROTO_UDP)) {
		if (send(fd, response->token - response->hdr_size, response->used_size + response->hdr_size, 0) <= 0) {
			cc_log_error("Failed to send data");
			result = CC_FAIL;
		}
	} else {
		cc_log_error("Failed to encode header");
		result = CC_FAIL;
	}

	coap_delete_pdu(response);

	return result;
}

static cc_result_t cc_coap_read(struct cc_calvinsys_obj_t *obj, char **data, size_t *size)
{
	cc_result_t result = CC_SUCCESS;
	cc_coap_state_t *state = (cc_coap_state_t *)obj->state;
	uint8_t buffer[512] = {0};
	size_t payload_len = 0;
	uint8_t *payload = NULL;
	int n = 0;
	coap_pdu_t *pdu = NULL;
	char *w = NULL;

	n = recv(state->fd, buffer, 512, 0);
	if (n < 0) {
		cc_log_error("Failed to read data");
		return CC_FAIL;
	}

	FD_CLR(state->fd, &obj->capability->calvinsys->node->fds);

	pdu = coap_pdu_init(0, 0, 0, n - 4);
	if (!pdu) {
		cc_log_error("Failed to init PDU");
		return CC_FAIL;
	}

	if (coap_pdu_parse(COAP_PROTO_UDP, buffer, n, pdu)) {
		if (COAP_RESPONSE_CLASS(pdu->code) == 2) {
		} else {
			cc_log_error("Request failed");
			result = CC_FAIL;
		}

		if (pdu->type == COAP_MESSAGE_CON) {
			if (cc_coap_send_ack(state->fd, pdu) != CC_SUCCESS)
				cc_log_error("Failed to send ack");
		}

		if (coap_get_data(pdu, &payload_len, &payload)) {
			if (cc_platform_mem_alloc((void **)data, cc_coder_sizeof_str(payload_len)) == CC_SUCCESS) {
				w = *data;
				w = cc_coder_encode_str(w, (char *)payload, payload_len);
				if (w == NULL) {
					cc_log_error("Failed to encode payload");
					cc_platform_mem_free(*data);
					result = CC_FAIL;
				}
				*size = w - *data;
			} else {
				cc_log_error("Failed to allocate memory");
				result = CC_FAIL;
			}
		} else {
			cc_log_error("Failed to get data");
			result = CC_FAIL;
		}
	} else {
		cc_log_error("Failed to parse DPU");
		result = CC_FAIL;
	}

	coap_delete_pdu(pdu);

	return result;
}

static bool cc_coap_can_write(struct cc_calvinsys_obj_t *obj)
{
	return true;
}

static cc_result_t cc_coap_write(struct cc_calvinsys_obj_t *obj, char *data, size_t size)
{
	return cc_coap_send_pdu(obj, true);
}

static cc_result_t cc_coap_close(struct cc_calvinsys_obj_t *obj)
{
	cc_coap_state_t *state = (cc_coap_state_t *)obj->state;
	cc_calvinsys_t *sys = obj->capability->calvinsys;
	int i = 0;

	for (i = 0; i < CC_CALVINSYS_MAX_FDS; i++) {
		if (sys->fds[i] == state->fd) {
			sys->fds[i] = -1;
			break;
		}
	}

	cc_coap_send_pdu(obj, false);
	close(state->fd);
	cc_platform_mem_free(state);

	return CC_SUCCESS;
}

static cc_result_t cc_coap_setup(int *fd, char *ip, char *port)
{
	cc_result_t result = CC_FAIL;
	struct addrinfo hints;
	struct addrinfo *list = NULL;
	struct addrinfo *node = NULL;
	int flags = 0;
	int ret = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;  // TODO: Support IPv6
	hints.ai_socktype = SOCK_DGRAM;

	ret = getaddrinfo(ip, port, &hints, &list);
	if (ret != 0) {
		cc_log_error("getaddrinfo failed '%d'", ret);
		return CC_FAIL;
	}

	for (node = list; node != NULL; node = node->ai_next) {
		if ((node->ai_family == AF_INET) && (node->ai_socktype == SOCK_DGRAM)) {
			*fd = socket(node->ai_family, node->ai_socktype, node->ai_protocol);
			if (*fd != -1) {
				if (connect(*fd, node->ai_addr, node->ai_addrlen) < 0) {
					cc_log_error("Failed to connect socket, errno '%d'", errno);
					close(*fd);
				} else {
					result = CC_SUCCESS;
					break;
				}
			} else
			cc_log_error("Failed to create socket");
		}
	}

	freeaddrinfo(list);

	if (result == CC_SUCCESS) {
		flags = fcntl(*fd, F_GETFL, 0);
		if (flags < 0) {
			cc_log_error("fcntl failed");
			close(*fd);
			return CC_FAIL;
		}

		ret = fcntl(*fd, F_SETFL, flags | O_NONBLOCK);
		if (ret < 0) {
			cc_log_error("fcntl failed");
			close(*fd);
			return CC_FAIL;
		}
	}

	return result;
}

static cc_result_t cc_coap_init(cc_coap_init_args_t *init_args, cc_coap_state_t *state)
{
	char ip[40] = {0};
	char port[10] = {0};

	if (coap_split_uri((unsigned char *)init_args->uri, strlen(init_args->uri), &state->uri) < 0) {
		cc_log_error("Invalid URI '%s'", init_args->uri);
		return CC_FAIL;
	}

	sprintf(port, "%d", state->uri.port);
	strncpy(ip, (char *)state->uri.host.s, state->uri.host.length);

	if (cc_coap_setup(&state->fd, ip, port) != CC_SUCCESS) {
		cc_log_error("Failed to setup COAP client");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static cc_result_t cc_calvinsys_coap_open(cc_calvinsys_obj_t *obj, cc_list_t *kwargs)
{
	cc_coap_state_t *state = NULL;
	cc_coap_init_args_t *init_args = (cc_coap_init_args_t *)obj->capability->init_args;
	cc_calvinsys_t *sys = obj->capability->calvinsys;
	int i = 0, fd_pos = -1;

	for (i = 0; i < CC_CALVINSYS_MAX_FDS; i++) {
		if (sys->fds[i] == -1) {
			fd_pos = i;
			break;
		}
	}

	if (fd_pos == -1) {
		cc_log_error("No free descriptors");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_coap_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}
	memset(state, 0, sizeof(cc_coap_state_t));

	if (cc_coap_init(init_args, state) != CC_SUCCESS) {
		cc_log_error("Failed to init coap client");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	obj->can_write = cc_coap_can_write;
	obj->write = cc_coap_write;
	obj->can_read = cc_coap_can_read;
	obj->read = cc_coap_read;
	obj->close = cc_coap_close;
	obj->state = state;
	sys->fds[fd_pos] = state->fd;

	return CC_SUCCESS;
}

cc_result_t cc_libcoap_create(cc_calvinsys_t *calvinsys, const char *args)
{
	int i = 0, j = 0, r = 0;
	jsmn_parser parser;
	jsmntok_t tokens[128], *obj_coap = NULL, *obj_actors = NULL, *obj_capabilities = NULL;
	char name[100] = {0}, requires[100] = {0};
	size_t len = 0;
	cc_coap_init_args_t *init_args = NULL;
	cc_actor_type_t *type = NULL;

	jsmn_init(&parser);
	r = jsmn_parse(&parser, args, strlen(args), tokens, sizeof(tokens) / sizeof(tokens[0]));
	if (r < 0) {
		cc_log_error("Failed to parse JSON: %d", r);
		return CC_FAIL;
	}

	if (r < 1 || tokens[0].type != JSMN_OBJECT) {
		cc_log_error("Object expected\n");
		return CC_FAIL;
	}

	obj_coap = cc_json_get_dict_value(args, &tokens[0], parser.toknext, "coap", 4);
	if (obj_coap == NULL) {
		cc_log_error("Failed to get 'coap'");
		return CC_FAIL;
	}

	obj_actors = cc_json_get_dict_value(args, obj_coap, obj_coap->size, "actors", 6);
	if (obj_actors == NULL) {
		cc_log_error("Failed to get 'actors'");
		return CC_FAIL;
	}

	for (i = 0, j = 0; i < obj_actors->size; i++) {
		if (cc_platform_mem_alloc((void **)&type, sizeof(cc_actor_type_t)) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		memset(type, 0, sizeof(cc_actor_type_t));

		if (cc_actor_coap_setup(type) != CC_SUCCESS) {
			cc_log_error("Failed to register actor type");
			cc_platform_mem_free(type);
			return CC_FAIL;
		}

		j++;
		len = obj_actors[j].end - obj_actors[j].start;
		strncpy(name, args + obj_actors[j].start, len);
		name[len] = '\0';
		j++;
		len = obj_actors[j].end - obj_actors[j].start;
		if (cc_platform_mem_alloc((void **)&type->requires, len + 1) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			cc_platform_mem_free(type);
			return CC_FAIL;
		}
		strncpy(type->requires, args + obj_actors[j].start, len);
		type->requires[len] = '\0';

		if (cc_list_add_n(&calvinsys->node->actor_types,
			name, strlen(name), type, sizeof(cc_actor_type_t *)) == NULL) {
			cc_log_error("Failed to add '%s'", name);
			cc_platform_mem_free(type);
			cc_platform_mem_free(requires);
			return CC_FAIL;
		}
	}

	obj_capabilities = cc_json_get_dict_value(args, obj_coap, obj_coap->size, "capabilities", 12);
	if (obj_capabilities == NULL) {
		cc_log_error("Failed to get 'capabilities'");
		return CC_FAIL;
	}

	for (i = 0, j = 0; i < obj_capabilities->size; i++) {
		if (cc_platform_mem_alloc((void **)&init_args, sizeof(cc_coap_init_args_t)) != CC_SUCCESS) {
			cc_log_error("Failed to allocate memory");
			return CC_FAIL;
		}
		memset(init_args, 0, sizeof(cc_coap_init_args_t));

		j++;
		len = obj_capabilities[j].end - obj_capabilities[j].start;
		strncpy(name, args + obj_capabilities[j].start, len);
		name[len] = '\0';
		j++;
		len = obj_capabilities[j].end - obj_capabilities[j].start;
		strncpy(init_args->uri, args + obj_capabilities[j].start, len);
		init_args->uri[len] = '\0';

		if (cc_calvinsys_create_capability(calvinsys, name, cc_calvinsys_coap_open, cc_calvinsys_coap_open, (void *)init_args, true) != CC_SUCCESS) {
			cc_log_error("Failed to add capability '%s'", name);
			cc_platform_mem_free(init_args);
			return CC_FAIL;
		}
	}

	return CC_SUCCESS;
}
