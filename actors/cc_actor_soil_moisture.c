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
#include "cc_actor_soil_moisture.h"
#include "runtime/north/cc_actor_store.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/north/cc_fifo.h"
#include "runtime/north/cc_token.h"
#include "runtime/north/cc_port.h"
#include "runtime/north/cc_common.h"
#include "calvinsys/common/cc_calvinsys_timer.h"

typedef struct cc_actor_soil_moisture_state_t {
	char soilmoisture[CC_UUID_BUFFER_SIZE];
	char timer[CC_UUID_BUFFER_SIZE];
} cc_actor_soil_moisture_state_t;

static cc_result_t cc_actor_soil_moisture_init(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	cc_actor_soil_moisture_state_t *state = NULL;
	char *obj_ref = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_soil_moisture_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	obj_ref = cc_calvinsys_open(*actor, "io.soilmoisture", NULL);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'io.soilmoisture'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	strncpy(state->soilmoisture, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

	obj_ref = cc_calvinsys_open(*actor, "sys.timer.once", managed_attributes);
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		cc_platform_mem_free((void *)state);
		return CC_FAIL;
	}
	strncpy(state->timer, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_soil_moisture_set_state(cc_actor_t **actor, cc_list_t *managed_attributes)
{
	cc_actor_soil_moisture_state_t *state = NULL;
	cc_list_t *item = NULL;
	char *obj_ref = NULL;
	uint32_t obj_ref_len = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_soil_moisture_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	item = cc_list_get(managed_attributes, "soilmoisture");
	if (item == NULL) {
		cc_log_error("Failed to get 'soilmoisture'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'soilmoisture'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	strncpy(state->soilmoisture, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

	item = cc_list_get(managed_attributes, "timer");
	if (item == NULL) {
		cc_log_error("Failed to get 'timer'");
		return CC_FAIL;
	}

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'timer'");
		cc_platform_mem_free(state);
		return CC_FAIL;
	}
	strncpy(state->timer, obj_ref, strnlen(obj_ref, CC_UUID_BUFFER_SIZE));

	(*actor)->instance_state = (void *)state;

	return CC_SUCCESS;
}

static bool cc_actor_soil_moisture_fire(struct cc_actor_t *actor)
{
	cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	cc_actor_soil_moisture_state_t *state = (cc_actor_soil_moisture_state_t *)actor->instance_state;
	char *data_temp = NULL, *data_bool = NULL;
	size_t size = 0;

	if (!cc_calvinsys_can_read(actor->calvinsys, state->timer) || !cc_calvinsys_can_read(actor->calvinsys, state->soilmoisture))
	 	return false;

	if (!cc_fifo_slots_available(outport->fifo, 1))
		return false;

	size = cc_coder_sizeof_bool(true);
	if (cc_platform_mem_alloc((void **)&data_bool, size) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return false;
	}

	if (cc_coder_encode_bool(data_bool, true) == NULL) {
		cc_log_error("Failed to encode value");
		cc_platform_mem_free((void *)data_bool);
		return false;
	}

	if (cc_calvinsys_write(actor->calvinsys, state->timer, data_bool, size) != CC_SUCCESS) {
		cc_log_error("Failed to set timer");
		cc_platform_mem_free((void *)data_bool);
		return false;
	}

	cc_platform_mem_free((void *)data_bool);

	if (cc_calvinsys_read(actor->calvinsys, state->soilmoisture, &data_temp, &size) != CC_SUCCESS)
		return false;

	if (cc_fifo_write(outport->fifo, data_temp, size) != CC_SUCCESS) {
		cc_platform_mem_free((void *)data_temp);
		return false;
	}

	return true;
}

static cc_result_t cc_actor_soil_moisture_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_soil_moisture_state_t *state = NULL;
	char *buffer = NULL, *w = NULL;

	if (actor->instance_state == NULL) {
		cc_log_error("Actor does not have a state");
		return CC_FAIL;
	}

	state = (cc_actor_soil_moisture_state_t *)actor->instance_state;

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->timer))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_str(w, state->timer, strlen(state->timer));

	if (cc_list_add_n(managed_attributes, "timer", 5, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'timer' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&buffer, cc_coder_sizeof_str(strlen(state->soilmoisture))) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	w = buffer;
	w = cc_coder_encode_str(w, state->soilmoisture, strlen(state->soilmoisture));

	if (cc_list_add_n(managed_attributes, "soilmoisture", 12, buffer, w - buffer) == NULL) {
		cc_log_error("Failed to add 'soilmoisture' to managed attributes");
		cc_platform_mem_free(buffer);
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

static void cc_actor_soil_moisture_free(cc_actor_t *actor)
{
	if (actor->instance_state != NULL)
		cc_platform_mem_free((void *)actor->instance_state);
}

cc_result_t cc_actor_soil_moisture_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	if (cc_list_add_n(requires, "io.soilmoisture", 15, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	if (cc_list_add_n(requires, "sys.timer.once", 14, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_soil_moisture_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_soil_moisture_init;
	type->set_state = cc_actor_soil_moisture_set_state;
	type->free_state = cc_actor_soil_moisture_free;
	type->fire_actor = cc_actor_soil_moisture_fire;
	type->get_managed_attributes = cc_actor_soil_moisture_get_attributes;
	type->get_requires = cc_actor_soil_moisture_get_requires;

	return CC_SUCCESS;
}
