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

typedef struct cc_actor_log_state_t {
	char *loglevel;
  size_t loglevel_size;
} cc_actor_log_state_t;

static cc_result_t cc_actor_log_init(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	cc_actor_log_state_t *state = NULL;
	cc_list_t *item = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_log_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_log_state_t));
	actor->instance_state = (void *)state;

	item = cc_list_get(managed_attributes, "loglevel");
	if (item == NULL) {
		cc_log_error("Failed to get 'loglevel'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->loglevel, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->loglevel, item->data, item->data_len);
	state->loglevel_size = item->data_len;

	return CC_SUCCESS;
}

static cc_result_t cc_actor_log_set_state(cc_actor_t *actor, cc_list_t *managed_attributes)
{
	cc_actor_log_state_t *state = NULL;
	cc_list_t *item = NULL;

	if (cc_platform_mem_alloc((void **)&state, sizeof(cc_actor_log_state_t)) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memset(state, 0, sizeof(cc_actor_log_state_t));
	actor->instance_state = (void *)state;

	item = cc_list_get(managed_attributes, "loglevel");
	if (item == NULL) {
		cc_log_error("Failed to get 'loglevel'");
		return CC_FAIL;
	}

	if (cc_platform_mem_alloc((void **)&state->loglevel, item->data_len) != CC_SUCCESS) {
		cc_log_error("Failed to allocate memory");
		return CC_FAIL;
	}

	memcpy(state->loglevel, item->data, item->data_len);
	state->loglevel_size = item->data_len;

	return CC_SUCCESS;
}

static bool cc_actor_log_fire(struct cc_actor_t *actor)
{
  cc_port_t *inport = (cc_port_t *)actor->in_ports->data;
	cc_token_t *token = NULL;
  char *str = NULL;
  uint32_t len = 0, u_num = 0;
  int32_t num = 0;
  bool b = false;
  float f = 0;
  double d = 0;
  cc_result_t result;

	if (!cc_fifo_tokens_available(inport->fifo, 1))
		return false;

	token = cc_fifo_peek(inport->fifo);

  switch (cc_coder_type_of(token->value)) {
	case CC_CODER_NIL:
    cc_log("ACTOR-%s: NIL", actor->id);
    break;
	case CC_CODER_STR:
		result = cc_coder_decode_str(token->value, &str, &len);
		if (result == CC_SUCCESS) {
      cc_log("ACTOR-%s: %.*s", actor->id, len, str);
		}
		break;
	case CC_CODER_UINT:
		result = cc_coder_decode_uint(token->value, &u_num);
		if (result == CC_SUCCESS) {
			cc_log("ACTOR-%s: %ld", actor->id, u_num);
		}
		break;
	case CC_CODER_INT:
		result = cc_coder_decode_int(token->value, &num);
		if (result == CC_SUCCESS) {
			cc_log("ACTOR-%s: %d", actor->id, num);
		}
		break;
	case CC_CODER_BOOL:
		result = cc_coder_decode_bool(token->value, &b);
		if (result == CC_SUCCESS) {
			cc_log("ACTOR-%s: %d", actor->id, b ? "true" : "false");
		}
		break;
	case CC_CODER_FLOAT:
		result = cc_coder_decode_float(token->value, &f);
		if (result == CC_SUCCESS) {
      cc_log("ACTOR-%s: %f", actor->id, f);
		}
		break;
	case CC_CODER_DOUBLE:
		result = cc_coder_decode_double(token->value, &d);
		if (result == CC_SUCCESS) {
      cc_log("ACTOR-%s: %d", actor->id, d);
		}
		break;
	default:
		cc_log("ACTOR-%s: unsupported type");
	}

	cc_fifo_commit_read(inport->fifo, true);

	return true;

}

static void cc_actor_log_free(cc_actor_t *actor)
{
	cc_actor_log_state_t *state = (cc_actor_log_state_t *)actor->instance_state;

	if (state != NULL) {
		if (state->loglevel != NULL)
			cc_platform_mem_free(state->loglevel);
		cc_platform_mem_free(state);
	}
}

static cc_result_t cc_actor_log_get_attributes(cc_actor_t *actor, cc_list_t **managed_attributes)
{
	cc_actor_log_state_t *state = (cc_actor_log_state_t *)actor->instance_state;

	if (cc_list_add_n(managed_attributes, "loglevel", 8, state->loglevel, state->loglevel_size) == NULL) {
		cc_log_error("Failed to add 'loglevel' to managed attributes");
		return CC_FAIL;
	}
	state->loglevel = NULL;

	return CC_SUCCESS;
}

cc_result_t cc_actor_log_get_requires(cc_actor_t *actor, cc_list_t **requires)
{
	return CC_SUCCESS;
}

cc_result_t cc_actor_log_setup(cc_actor_type_t *type)
{
	type->init = cc_actor_log_init;
	type->set_state = cc_actor_log_set_state;
	type->free_state = cc_actor_log_free;
	type->fire_actor = cc_actor_log_fire;
	type->get_managed_attributes = cc_actor_log_get_attributes;
	type->get_requires = cc_actor_log_get_requires;

	return CC_SUCCESS;
}
