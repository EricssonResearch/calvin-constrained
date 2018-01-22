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
#include <stdlib.h>
#include <string.h>
#include "runtime/north/cc_actor_store.h"
#include "runtime/north/cc_fifo.h"
#include "runtime/north/cc_token.h"
#include "runtime/north/cc_port.h"
#include "runtime/north/cc_common.h"
#include "runtime/north/coder/cc_coder.h"
#include "calvinsys/common/cc_calvinsys_timer.h"
#include "calvinsys/cc_calvinsys.h"

typedef struct cc_state_t {
	char level[CC_UUID_BUFFER_SIZE];
	char *period;
	size_t period_size;
} cc_state_t;

static cc_result_t cc_actor_accelerometer_init(cc_actor_t *actor, cc_list_t *attributes)
{
	cc_state_t *state = NULL;
	char *obj_ref = NULL;
	cc_list_t *item = NULL;

	item = cc_list_get(attributes, "period");
	if (item == NULL) {
		cc_log_error("Failed to get attribute 'period'");
		return CC_FAIL;
	}

	obj_ref = (void *)cc_calvinsys_open(actor, "io.accelerometer", attributes);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'io.accelerometer'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->period, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	memcpy(state->period, item->data, item->data_len);
	state->period_size = item->data_len;
	strncpy(state->level, obj_ref, strlen(obj_ref));
	state->level[strlen(obj_ref)] = '\0';
	actor->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_accelerometer_set_state(cc_actor_t *actor, cc_list_t *attributes)
{
	cc_state_t *state = NULL;
	cc_list_t *item = NULL;
	char *obj_ref = NULL;
	uint32_t obj_ref_len = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_state_t));
	actor->instance_state = (void *)state;

	item = cc_list_get(attributes, "level");
	if (item == NULL) {
		cc_log_error("Failed to get 'level'");
		return CC_FAIL;
	}

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'level'");
		return CC_FAIL;
	}
	strncpy(state->level, obj_ref, obj_ref_len);
	state->level[obj_ref_len] = '\0';

	item = cc_list_get(attributes, "period");
	if (item == NULL) {
		cc_log_error("Failed to get attribute 'period'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->period, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->period, item->data, item->data_len);
	state->period_size = item->data_len;

	return CC_SUCCESS;
}

static bool cc_actor_accelerometer_fire(cc_actor_t *actor)
{
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	char *obj_ref = (char *)actor->instance_state;
	char *data = NULL;
	size_t size = 0;

	if (!cc_calvinsys_can_read(actor->calvinsys, obj_ref))
	 	return false;

	if (!cc_fifo_slots_available(outport->fifo, 1))
		return false;

	if (cc_calvinsys_read(actor->calvinsys, obj_ref, &data, &size) != CC_SUCCESS) {
		cc_log_error("Failed to read data");
		return false;
	}

	if (cc_fifo_write(outport->fifo, data, size) != CC_SUCCESS) {
		cc_log_error("Failed to write data to fifo");
		cc_platform_mem_free((void *)data);
		return false;
	}

	return true;
}

static void cc_actor_accelerometer_free(cc_actor_t *actor)
{
	cc_state_t *state = NULL;

	if (actor->instance_state != NULL) {
		state = (cc_state_t *)actor->instance_state;
		if (state->period != NULL)
			cc_platform_mem_free(state->period);
		cc_platform_mem_free(state);
	}
}

static cc_result_t cc_actor_accelerometer_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_state_t *state = (cc_state_t *)actor->instance_state;
	char *buffer = NULL, *w = NULL;

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->level))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_str(w, state->level, strlen(state->level));

	if (cc_list_add_n(managed_attributes, "level", 5, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'level' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	if (cc_list_add_n(managed_attributes, "period", 6, state->period, state->period_size) == NULL) {
		cc_log_error("Failed to add 'period' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}
	state->period = NULL;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_accelerometer_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "io.accelerometer", 16, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_accelerometer_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_accelerometer_init;
	type->set_state = cc_actor_accelerometer_set_state;
	type->fire_actor = cc_actor_accelerometer_fire;
	type->get_requires = cc_actor_accelerometer_get_requires;
	type->free_state = cc_actor_accelerometer_free;
	type->get_managed_attributes = cc_actor_accelerometer_get_attributes;

	return CC_SUCCESS;
}
