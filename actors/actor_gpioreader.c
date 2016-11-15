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
#include "actor_gpioreader.h"
#include "../msgpack_helper.h"
#include "../fifo.h"
#include "../token.h"

result_t actor_gpioreader_init(actor_t **actor, char *obj_actor_state)
{
    state_gpioreader_t *state = NULL;

    if (platform_mem_alloc((void **)&state, sizeof(state_gpioreader_t)) != SUCCESS) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    memset(state, 0, sizeof(state_gpioreader_t));

    if (actor_get_managed(obj_actor_state, &state->managed_attributes) != SUCCESS) {
        platform_mem_free((void *)state);
        return FAIL;
    }

    state->pin = 12;
    state->pull[0] = 'd';
    state->edge[0] = 'b';
    state->gpio = platform_create_in_gpio(state->pin, state->pull[0], state->edge[0]);
    if (state->gpio == NULL) {
        log_error("Faield create gpio");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }

    (*actor)->instance_state = (void *)state;

    return SUCCESS;
}

result_t actor_gpioreader_set_state(actor_t **actor, char *obj_actor_state)
{
    result_t result = SUCCESS;
    state_gpioreader_t *state = NULL;
    char *tmp_string = NULL;
    uint32_t tmp_string_len = 0;
    list_t *list = NULL;

    if (platform_mem_alloc((void **)&state, sizeof(state_gpioreader_t)) != SUCCESS) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    if (actor_get_managed(obj_actor_state, &state->managed_attributes) != SUCCESS) {
        platform_mem_free((void *)state);
        return FAIL;
    }
    
    list = state->managed_attributes;
    while (list != NULL && result == SUCCESS) {
        if (strncmp(list->id, "gpio_pin", strlen("gpio_pin")) == 0) {
            result = decode_uint((char *)list->data, &state->pin);
        } else if (strncmp(list->id, "pull", strlen("pull")) == 0) {
            result = decode_str((char *)list->data, &tmp_string, &tmp_string_len);
            if (result == SUCCESS)
                state->pull[0] = tmp_string[0];
        } else if (strncmp(list->id, "edge", strlen("edge")) == 0) {
            result = decode_str((char *)list->data, &tmp_string, &tmp_string_len);
            if (result == SUCCESS)
                state->edge[0] = tmp_string[0];
        }
        list = list->next;
    }
    
    if (result != SUCCESS) {
        log_error("Failed to parse attributes");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }

    state->gpio = platform_create_in_gpio(state->pin, state->pull[0], state->edge[0]);
    if (state->gpio == NULL) {
        log_error("Faield create gpio");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }
    
    (*actor)->instance_state = (void *)state;

    return SUCCESS; 
}

result_t actor_gpioreader_fire(struct actor_t *actor)
{
    token_t out_token;
    port_t *outport = NULL;
    state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;

    if (!state->gpio->has_triggered)
        return FAIL;

    outport = port_get_from_name(actor, "state", PORT_DIRECTION_OUT);
    if (outport == NULL) {
        log_error("No port with name 'state'");
        return FAIL;
    }

    if (fifo_slots_available(&outport->fifo, 1) == 1) {
        token_set_uint(&out_token, state->gpio->value);

        if (fifo_write(&outport->fifo, out_token.value, out_token.size) != SUCCESS) {
            log_error("Failed to write token");
            return FAIL;
        }
    }

    return SUCCESS;
}

void actor_gpioreader_free(actor_t *actor)
{
    state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;

    if (state != NULL) {
        if (state->gpio != NULL)
            platform_uninit_gpio(state->gpio);
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
    }
}

char *actor_gpioreader_serialize(actor_t *actor, char **buffer)
{
    state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;

    *buffer = actor_serialize_managed_list(state->managed_attributes, buffer);
    if (list_get(state->managed_attributes, "gpio_pin") == NULL)            
        *buffer = encode_uint(buffer, "gpio_pin", state->gpio->pin);
    if (list_get(state->managed_attributes, "pull") == NULL)
        *buffer = encode_str(buffer, "pull", state->pull, 1);
    if (list_get(state->managed_attributes, "edge") == NULL)
        *buffer = encode_str(buffer, "edge", state->edge, 1);

    return *buffer;
}

list_t *actor_gpioreader_get_managed_attributes(actor_t *actor)
{
    state_gpioreader_t *state = (state_gpioreader_t *)actor->instance_state;
    
    return state->managed_attributes;
}
