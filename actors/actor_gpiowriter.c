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
#include "actor_gpiowriter.h"
#include "../msgpack_helper.h"
#include "../fifo.h"

result_t actor_gpiowriter_init(char *obj_actor_state, actor_state_t **state)
{
    result_t result = SUCCESS;
    state_gpiowriter_t *gpiowriter_state = NULL;
    char *obj_shadow_args = NULL;
    uint32_t pin = 0;

    *state = (actor_state_t *)malloc(sizeof(actor_state_t));
    if (*state == NULL) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    gpiowriter_state = (state_gpiowriter_t *)malloc(sizeof(state_gpiowriter_t));
    if (gpiowriter_state == NULL) {
        log_error("Failed to allocate memory");
        free(*state);
        return FAIL;
    }

    gpiowriter_state->gpio = NULL;

    if (has_key(&obj_actor_state, "_shadow_args")) {
        result = get_value_from_map(&obj_actor_state, "_shadow_args", &obj_shadow_args);

        if (result == SUCCESS)
            result = decode_uint_from_map(&obj_shadow_args, "gpio_pin", &pin);
    } else {
        if (result == SUCCESS)
            result = decode_uint_from_map(&obj_actor_state, "gpio_pin", &pin);
    }

    if (result == SUCCESS) {
        gpiowriter_state->gpio = create_out_gpio(pin);
        if (gpiowriter_state->gpio == NULL)
            result = FAIL;
        else {
            (*state)->nbr_attributes = 1;
            (*state)->state = (void *)gpiowriter_state;
        }
    }

    if (result != SUCCESS) {
        free(*state);
        if (gpiowriter_state != NULL) 
            free(gpiowriter_state);
    }

    return result;
}

result_t actor_gpiowriter_fire(struct actor_t *actor)
{
    result_t result = SUCCESS;
    token_t *token = NULL;
    uint32_t value;
    state_gpiowriter_t *gpio_state = NULL;

    while (fifo_can_read(actor->inports->fifo) && result == SUCCESS) {
        token = fifo_read(actor->inports->fifo);
        result = decode_uint_token(token, &value);
        if (result == SUCCESS) {
            fifo_commit_read(actor->inports->fifo, true, true);                
            gpio_state = (state_gpiowriter_t *)actor->state->state;
            set_gpio(gpio_state->gpio, value);
        } else
            fifo_commit_read(actor->inports->fifo, false, false);
    }

    return result;
}

void actor_gpiowriter_free(actor_t *actor)
{
    state_gpiowriter_t *state = NULL;

    if (actor->state != NULL) {
        if (actor->state->state != NULL) {
            state = (state_gpiowriter_t *)actor->state->state;
            if (state->gpio != NULL)
                uninit_gpio(state->gpio);
            free(state);
        }
        free(actor->state);
    }
}

char *actor_gpiowriter_serialize(actor_state_t *state, char **buffer)
{
    state_gpiowriter_t *gpio_state = NULL;

    if (state != NULL && state->state != NULL) {
        gpio_state = (state_gpiowriter_t *)state->state;
        *buffer = encode_uint(buffer, "gpio_pin", gpio_state->gpio->pin);
    }

    return *buffer;
}
