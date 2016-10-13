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
#include "actor_gpioreader.h"
#include "../msgpack_helper.h"
#include "../fifo.h"
#include "../token.h"

result_t actor_gpioreader_init(actor_t **actor, char *obj_actor_state, actor_state_t **state)
{
    result_t result = SUCCESS;
    state_gpioreader_t *gpioreader_state = NULL;
    char *obj_shadow_args = NULL;
    uint32_t pin = 0;

    *state = (actor_state_t *)malloc(sizeof(actor_state_t));
    if (*state == NULL) {
        log_error("Failed to allocate memory");
        return FAIL;
    }

    gpioreader_state = (state_gpioreader_t *)malloc(sizeof(state_gpioreader_t));
    if (gpioreader_state == NULL) {
        log_error("Failed to allocate memory");
        free(*state);
        return FAIL;
    }

    gpioreader_state->gpio = NULL;
    gpioreader_state->pull = NULL;
    gpioreader_state->edge = NULL;

    if (has_key(&obj_actor_state, "_shadow_args")) {
        result = get_value_from_map(&obj_actor_state, "_shadow_args", &obj_shadow_args);

        if (result == SUCCESS)
            result = decode_uint_from_map(&obj_shadow_args, "gpio_pin", &pin);

        if (result == SUCCESS)
            result = decode_string_from_map(&obj_shadow_args, "pull", &gpioreader_state->pull);

        if (result == SUCCESS)
            result = decode_string_from_map(&obj_shadow_args, "edge", &gpioreader_state->edge);
    } else {
        if (result == SUCCESS)
            result = decode_uint_from_map(&obj_actor_state, "gpio_pin", &pin);

        if (result == SUCCESS)
            result = decode_string_from_map(&obj_actor_state, "pull", &gpioreader_state->pull);

        if (result == SUCCESS)
            result = decode_string_from_map(&obj_actor_state, "edge", &gpioreader_state->edge);
    }

    if (result == SUCCESS) {
        gpioreader_state->gpio = create_in_gpio(pin, gpioreader_state->pull[0], gpioreader_state->edge[0]);
        if (gpioreader_state->gpio == NULL)
            result = FAIL;
        else {
            result = add_managed_attribute(actor, "gpio_pin");

            if (result == SUCCESS)
                result = add_managed_attribute(actor, "pull");

            if (result == SUCCESS)
                result = add_managed_attribute(actor, "edge");

            (*state)->state = (void *)gpioreader_state;
        }
    }

    if (result != SUCCESS) {
        free(*state);
        if (gpioreader_state != NULL) {
            if (gpioreader_state->pull != NULL)
                free(gpioreader_state->pull);

            if (gpioreader_state->edge != NULL)
                free(gpioreader_state->edge);

            free(gpioreader_state);
        }
    }

    return result;
}

result_t actor_gpioreader_fire(struct actor_t *actor)
{
    result_t result = SUCCESS;
    token_t *token = NULL;
    state_gpioreader_t *state = (state_gpioreader_t *)actor->state->state;

    if (fifo_can_write(actor->outports->fifo) && state->gpio->has_triggered) {
        result = create_uint_token(state->gpio->value, &token);
        if (result == SUCCESS) {
            result = fifo_write(actor->outports->fifo, token);
            if (result == SUCCESS)
                state->gpio->has_triggered = false;
            else {
                log_error("Failed to write to fifo");
                free_token(token);
            }
        } else
            log_error("Failed to create token");
    }

    return result;
}

void actor_gpioreader_free(actor_t *actor)
{
    state_gpioreader_t *state = NULL;

    if (actor->state != NULL) {
        if (actor->state->state != NULL) {
            state = (state_gpioreader_t *)actor->state->state;
            if (state->gpio != NULL)
                uninit_gpio(state->gpio);
            free(state->pull);
            free(state->edge);
            free(state);
        }
        free(actor->state);
    }
}

char *actor_gpioreader_serialize(actor_state_t *state, char **buffer)
{
    state_gpioreader_t *gpio_state = NULL;

    if (state != NULL && state->state != NULL) {
        gpio_state = (state_gpioreader_t *)state->state;
        *buffer = encode_uint(buffer, "gpio_pin", gpio_state->gpio->pin);
        *buffer = encode_str(buffer, "pull", gpio_state->pull);
        *buffer = encode_str(buffer, "edge", gpio_state->edge);
    }

    return *buffer;
}
