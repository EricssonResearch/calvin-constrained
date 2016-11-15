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
#include "actor_gpiowriter.h"
#include "../msgpack_helper.h"
#include "../fifo.h"

result_t actor_gpiowriter_init(actor_t **actor, char *obj_actor_state)
{
    state_gpiowriter_t *state = NULL;

    if (platform_mem_alloc((void **)&state, sizeof(state_gpiowriter_t)) != SUCCESS) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    memset(state, 0, sizeof(state_gpiowriter_t));

    if (actor_get_managed(obj_actor_state, &state->managed_attributes) != SUCCESS) {
        platform_mem_free((void *)state);
        return FAIL;
    }

    state->pin = 12;
    state->gpio = platform_create_out_gpio(state->pin);
    if (state->gpio == NULL) {
        log_error("Faield create gpio");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }

    (*actor)->instance_state = (void *)state;

    return SUCCESS;
}

result_t actor_gpiowriter_set_state(actor_t **actor, char *obj_actor_state)
{
    result_t result = SUCCESS;
    state_gpiowriter_t *state = NULL;
    list_t *list = NULL;

    if (platform_mem_alloc((void **)&state, sizeof(state_gpiowriter_t)) != SUCCESS) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    if (actor_get_managed(obj_actor_state, &state->managed_attributes) != SUCCESS) {
        platform_mem_free((void *)state);
        return FAIL;
    }
    
    list = state->managed_attributes;
    while (list != NULL && result == SUCCESS) {
        if (strncmp(list->id, "gpio_pin", strlen("gpio_pin")) == 0)
            result = decode_uint((char *)list->data, &state->pin);
        list = list->next;
    }
    
    if (result != SUCCESS) {
        log_error("Failed to parse attributes");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }

    state->gpio = platform_create_out_gpio(state->pin);
    if (state->gpio == NULL) {
        log_error("Faield create gpio");
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
        return FAIL;
    }
    
    (*actor)->instance_state = (void *)state;

    return SUCCESS; 
}

result_t actor_gpiowriter_fire(struct actor_t *actor)
{
    port_t *inport = NULL;
    token_t *in_token = NULL;
    uint32_t in_data = 0;
    bool did_fire = false;
    state_gpiowriter_t *gpio_state = NULL;

    inport = port_get_from_name(actor, "state", PORT_DIRECTION_IN);
    if (inport == NULL) {
        log_error("No port with name 'state'");
        return FAIL;
    }

    if (fifo_tokens_available(&inport->fifo, 1) == 1) {
        in_token = fifo_peek(&inport->fifo);

        if (token_decode_uint(*in_token, &in_data) != SUCCESS) {
            log_error("Failed to decode token");
            fifo_cancel_commit(&inport->fifo);
            return FAIL;
        }

        gpio_state = (state_gpiowriter_t *)actor->state;
        platform_set_gpio(gpio_state->gpio, in_data);

        fifo_commit_read(&inport->fifo);
        did_fire = true;
    }

    if (did_fire)
        return SUCCESS;

    return FAIL;
}

void actor_gpiowriter_free(actor_t *actor)
{
    state_gpiowriter_t *state = (state_gpiowriter_t *)actor->instance_state;

    if (state != NULL) {
        if (state->gpio != NULL)
            platform_uninit_gpio(state->gpio);
        actor_free_managed(state->managed_attributes);
        platform_mem_free((void *)state);
    }
}

char *actor_gpiowriter_serialize(actor_t *actor, char **buffer)
{
    state_gpiowriter_t *state = (state_gpiowriter_t *)actor->instance_state;

    *buffer = actor_serialize_managed_list(state->managed_attributes, buffer);
    if (list_get(state->managed_attributes, "gpio_pin") == NULL)
        *buffer = encode_uint(buffer, "gpio_pin", state->gpio->pin);
 
    return *buffer;
}

list_t *actor_gpiowriter_get_managed_attributes(actor_t *actor)
{
    state_gpiowriter_t *state = (state_gpiowriter_t *)actor->instance_state;
    
    return state->managed_attributes;
}
