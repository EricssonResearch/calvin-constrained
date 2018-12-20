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

typedef struct cc_actor_counttimer_state_t {
	char timer[CC_UUID_BUFFER_SIZE];
	char *sleep;
	size_t sleep_size;
  char *start;
	size_t start_size;
  char *steps;
	size_t steps_size;
  uint32_t count;
  bool stopped;
} cc_actor_counttimer_state_t;

static cc_result_t cc_actor_counttimer_init(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	cc_actor_counttimer_state_t *state = NULL;
	char *obj_ref = NULL;
	cc_list_t *item = NULL, *attributes = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_counttimer_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_counttimer_state_t));
	actor->instance_state = (void *)state;

	item = cc_list_get(managed_attributes, "sleep");
	if (item == NULL) {
		cc_log_error("Failed to get 'sleep'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->sleep, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->sleep, item->data, item->data_len);
	state->sleep_size = item->data_len;

	if (cc_list_add(&attributes, "sleep", state->sleep, state->sleep_size) == NULL) {
		cc_log_error("Failed to add 'sleep'");
		return CC_FAIL;
	}

  item = cc_list_get(managed_attributes, "start");
	if (item == NULL) {
		cc_log_error("Failed to get 'start'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->start, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->start, item->data, item->data_len);
	state->start_size = item->data_len;

	if (cc_list_add(&attributes, "start", state->start, state->start_size) == NULL) {
		cc_log_error("Failed to add 'start'");
		return CC_FAIL;
	}

  item = cc_list_get(managed_attributes, "steps");
	if (item == NULL) {
		cc_log_error("Failed to get 'steps'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->steps, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->steps, item->data, item->data_len);
	state->steps_size = item->data_len;

	if (cc_list_add(&attributes, "period", state->sleep, state->sleep_size) == NULL) {
		cc_log_error("Failed to add 'sleep'");
		return CC_FAIL;
	}

	obj_ref = cc_calvinsys_open(actor, "sys.timer.once", attributes);
	cc_list_remove(&attributes, "period");
	if (obj_ref == NULL) {
		cc_log_error("Failed to open 'sys.timer.once'");
		return CC_FAIL;
	}
	strncpy(state->timer, obj_ref, strlen(obj_ref));
	state->timer[strlen(obj_ref)] = '\0';

  item = cc_list_get(managed_attributes, "count");
  if (item == NULL) {
    cc_log_error("Failed to get 'count'");
    return CC_FAIL;
  }

  if (cc_coder_decode_uint(item->data, &state->count) != CC_SUCCESS) {
    cc_log_error("Failed to decode 'count'");
    return false;
  }

  state->stopped = false;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_counttimer_set_state(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	cc_actor_counttimer_state_t *state = NULL;
	cc_list_t *item = NULL;
	char *obj_ref = NULL;
	uint32_t obj_ref_len = 0;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_counttimer_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_counttimer_state_t));
	actor->instance_state = (void *)state;

	item = cc_list_get(managed_attributes, "timer");
	if (item == NULL) {
		cc_log_error("Failed to get 'timer'");
		return CC_FAIL;
	}

	if (cc_coder_decode_str((char *)item->data, &obj_ref, &obj_ref_len) != CC_SUCCESS) {
		cc_log_error("Failed to decode 'timer'");
		return CC_FAIL;
	}
	strncpy(state->timer, obj_ref, obj_ref_len);
	state->timer[obj_ref_len] = '\0';

	item = cc_list_get(managed_attributes, "sleep");
	if (item == NULL) {
		cc_log_error("Failed to get attribute 'sleep'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->sleep, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->sleep, item->data, item->data_len);
	state->sleep_size = item->data_len;

  item = cc_list_get(managed_attributes, "start");
  if (item == NULL) {
    cc_log_error("Failed to get attribute 'start'");
    return CC_FAIL;
  }

  if (cc_platform_mem_alloc((void **)&state->start, item->data_len) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_FAIL;
  }

  memcpy(state->start, item->data, item->data_len);
  state->start_size = item->data_len;

  item = cc_list_get(managed_attributes, "steps");
  if (item == NULL) {
    cc_log_error("Failed to get attribute 'steps'");
    return CC_FAIL;
  }

  if (cc_platform_mem_alloc((void **)&state->steps, item->data_len) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_FAIL;
  }

  memcpy(state->steps, item->data, item->data_len);
  state->steps_size = item->data_len;

  item = cc_list_get(managed_attributes, "count");
  if (item == NULL) {
    cc_log_error("Failed to get attribute 'count'");
    return CC_FAIL;
  }

  if (cc_coder_decode_uint(item->data, &state->count) != CC_SUCCESS) {
    cc_log_error("Failed to decode 'count'");
    return false;
  }

  state->stopped = false;

	return CC_SUCCESS;
}

static bool cc_actor_counttimer_step_no_periodic(struct cc_actor_t *actor)
{
  cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	cc_actor_counttimer_state_t *state = (cc_actor_counttimer_state_t *)actor->instance_state;
  uint32_t steps = 0, start = 0;
  size_t size = 0;
  char *data = NULL, *obj_ref = NULL;
  cc_list_t *attributes = NULL;

  if (cc_coder_decode_uint(state->steps, &steps) != CC_SUCCESS) {
    cc_log_error("Failed to decode steps");
    return false;
  }

  if (cc_coder_decode_uint(state->start, &start) != CC_SUCCESS) {
    cc_log_error("Failed to decode start");
    return false;
  }

  if (state->count >= start + 3 || state->count >= steps || !cc_calvinsys_can_read(actor->calvinsys, state->timer)) {
    return false;
  }

  cc_log("Firing cc_actor_counttimer_step_no_periodic start: %ld count: %ld steps: %ld", start, state->count, steps);

  cc_calvinsys_read(actor->calvinsys, state->timer, &data, &size);

  if (state->count == start + 2) {
    cc_calvinsys_close(actor->calvinsys, state->timer);

    if (cc_list_add(&attributes, "period", state->sleep, state->sleep_size) == NULL) {
      cc_log_error("Failed to add 'sleep'");
      return CC_FAIL;
    }

    obj_ref = cc_calvinsys_open(actor, "sys.timer.repeating", attributes);
    cc_list_remove(&attributes, "period");
    if (obj_ref == NULL) {
    	cc_log_error("Failed to open 'sys.timer.repeating'");
    	return CC_FAIL;
    }
    strncpy(state->timer, obj_ref, strlen(obj_ref));
    state->timer[strlen(obj_ref)] = '\0';
  }

  size = cc_coder_sizeof_uint(state->count);
  if (cc_platform_mem_alloc((void **)&data, size) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return false;
  }
  cc_coder_encode_uint(data, state->count);

  if (cc_fifo_write(outport->fifo, data, size) != CC_SUCCESS) {
    cc_platform_mem_free((void *)data);
    return false;
  }

  if (cc_calvinsys_write(actor->calvinsys, state->timer, state->sleep, state->sleep_size) != CC_SUCCESS)
    cc_log_debug("Failed to set timer %s", state->timer);

  state->count++;

	return true;
}

static bool cc_actor_counttimer_step_periodic(struct cc_actor_t *actor)
{
  cc_port_t *outport = (cc_port_t *)actor->out_ports->data;
	cc_actor_counttimer_state_t *state = (cc_actor_counttimer_state_t *)actor->instance_state;
  uint32_t steps = 0;
  size_t size = 0;
  char *data = NULL;

  if (cc_coder_decode_uint(state->steps, &steps) != CC_SUCCESS) {
    cc_log_error("Failed to decode steps");
    return false;
  }

  if (state->count >= steps || !cc_calvinsys_can_read(actor->calvinsys, state->timer)) {
    return false;
  }

  cc_log("Firing step_periodic count: %ld steps: %ld", state->count, state->steps);

  cc_calvinsys_read(actor->calvinsys, state->timer, &data, &size);

  size = cc_coder_sizeof_uint(state->count);
  if (cc_platform_mem_alloc((void **)&data, size) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return false;
  }
  cc_coder_encode_uint(data, state->count);

  if (cc_fifo_write(outport->fifo, data, size) != CC_SUCCESS) {
    cc_platform_mem_free((void *)data);
    return false;
  }

  state->count++;

	return true;
}

static bool cc_actor_counttimer_stop(struct cc_actor_t *actor)
{
	cc_actor_counttimer_state_t *state = (cc_actor_counttimer_state_t *)actor->instance_state;
  uint32_t steps = 0;

  if (state->stopped == true)
    return false;

  if (cc_coder_decode_uint(state->steps, &steps) != CC_SUCCESS) {
    cc_log_error("Failed to decode steps");
    return false;
  }

  if (state->count == steps) {
    cc_log("Fired stop count: %ld steps %ld", state->count, steps);
    cc_calvinsys_close(actor->calvinsys, state->timer);
    state->stopped = true;
    return true;
  }

  return false;
}

static bool cc_actor_counttimer_fire(struct cc_actor_t *actor)
{
	bool fired = false;

	if (cc_actor_counttimer_step_no_periodic(actor))
		fired = true;

	if (cc_actor_counttimer_step_periodic(actor))
		fired = true;

  if (cc_actor_counttimer_stop(actor))
  	fired = true;

	return fired;
}

static void cc_actor_counttimer_free(cc_actor_t *actor)
{
	cc_actor_counttimer_state_t *state = (cc_actor_counttimer_state_t *)actor->instance_state;

	if (state != NULL) {
		if (state->sleep != NULL)
			cc_platform_mem_free(state->sleep);
    if (state->start != NULL)
  		cc_platform_mem_free(state->start);
    if (state->steps != NULL)
    	cc_platform_mem_free(state->steps);
		cc_platform_mem_free(state);
	}
}

static cc_result_t cc_actor_counttimer_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_counttimer_state_t *state = (cc_actor_counttimer_state_t *)actor->instance_state;
	char *buffer = NULL, *w = NULL, *data = NULL;
  size_t size = 0;

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

	if (cc_list_add_n(managed_attributes, "sleep", 5, state->sleep, state->sleep_size) == NULL) {
		cc_log_error("Failed to add 'sleep' to managed attributes");
		return CC_FAIL;
	}
	state->sleep = NULL;

  if (cc_list_add_n(managed_attributes, "start", 5, state->start, state->start_size) == NULL) {
		cc_log_error("Failed to add 'start' to managed attributes");
		return CC_FAIL;
	}
	state->start = NULL;

  if (cc_list_add_n(managed_attributes, "steps", 5, state->steps, state->steps_size) == NULL) {
		cc_log_error("Failed to add 'steps' to managed attributes");
		return CC_FAIL;
	}
	state->steps = NULL;

  size = cc_coder_sizeof_uint(state->count);
  if (cc_platform_mem_alloc((void **)&data, size) != CC_SUCCESS) {
    cc_log_error("Failed to allocate memory");
    return CC_FAIL;
  }
  cc_coder_encode_uint(data, state->count);

  if (cc_list_add_n(managed_attributes, "count", 5, data, size) == NULL) {
		cc_log_error("Failed to add 'count' to managed attributes");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_counttimer_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
  if (cc_list_add_n(requires, "sys.timer.once", 14, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	if (cc_list_add_n(requires, "sys.timer.repeating", 19, NULL, 0) == NULL) {
		cc_log_error("Failed to add requires");
		return CC_FAIL;
	}

	return CC_SUCCESS;
}

cc_result_t cc_actor_counttimer_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_counttimer_init;
	type->set_state = cc_actor_counttimer_set_state;
	type->free_state = cc_actor_counttimer_free;
	type->fire_actor = cc_actor_counttimer_fire;
	type->get_managed_attributes = cc_actor_counttimer_get_attributes;
	type->get_requires = cc_actor_counttimer_get_requires;

	return CC_SUCCESS;
}
