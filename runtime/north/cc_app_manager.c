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
#include <string.h>
#include <stdio.h>
#include "cc_app_manager.h"
#include "runtime/north/cc_node.h"
#include "runtime/north/coder/cc_coder.h"
#include "runtime/north/cc_actor.h"
#include "runtime/north/cc_port.h"
#include "runtime/south/platform/cc_platform.h"
#include "runtime/north/cc_proto.h"

static cc_result_t cc_app_manager_setup_port_connections(cc_node_t *node, cc_port_t *port, char *obj_connections)
{
  cc_result_t result = CC_SUCCESS;
  char *connection_name = NULL, *delimiter = NULL;
  uint32_t connection_name_len = 0;
  uint32_t i = 0, connection_count = cc_coder_get_size_of_array(obj_connections);
  uint32_t dest_actor_name_len = 0;
  cc_actor_t *dest_actor = NULL;
  cc_port_t *dest_port = NULL;

  for (i = 0; i < connection_count && result == CC_SUCCESS; i++) {
    if ((result = cc_coder_decode_string_from_array(obj_connections, i, &connection_name, &connection_name_len)) != CC_SUCCESS)
      cc_log_error("Failed to decode connection");
    else {
      delimiter = strchr(connection_name, '.');
      dest_actor_name_len = delimiter - connection_name;
      dest_actor = cc_actor_get_from_name(node, connection_name, dest_actor_name_len);
      if (dest_actor == NULL) {
        cc_log_error("Failed to get peer actor '%.*s'", dest_actor_name_len, connection_name);
        result = CC_FAIL;
        break;
      }
      dest_port = cc_port_get_from_name(dest_actor, connection_name + dest_actor_name_len + 1, connection_name_len - (dest_actor_name_len + 1), CC_PORT_DIRECTION_IN);
      if (dest_port == NULL) {
        cc_log_error("Failed to get peer port");
        result = CC_FAIL;
        break;
      }

      strncpy(port->peer_port_id, dest_port->id, CC_UUID_BUFFER_SIZE);
      result = cc_port_connect_local(node, port);
    }
  }

  return result;
}

static cc_result_t cc_app_manager_create_ports(cc_node_t *node, cc_actor_t *actor, char *obj_portproperties)
{
  cc_result_t result = CC_SUCCESS;
  uint32_t i = 0, port_count = 0, name_len = 0, direction_len = 0, nbr_peers = 0;
  char *name = NULL, *direction = NULL, *obj_port = NULL, *obj_properties = NULL;
  cc_port_t *port = NULL;

  port_count = cc_coder_get_size_of_array(obj_portproperties);
  for (i = 0; i < port_count && result == CC_SUCCESS; i++) {
    if (result == CC_SUCCESS && (result = cc_coder_get_value_from_array(obj_portproperties, i, &obj_port)) != CC_SUCCESS)
      cc_log_error("Failed to decode port property '%ld'", i);

    if (result == CC_SUCCESS && (result = cc_coder_decode_string_from_map(obj_port, "port", &name, &name_len)) != CC_SUCCESS)
      cc_log_error("Failed to decode 'port'");

    if (result == CC_SUCCESS && (result = cc_coder_decode_string_from_map(obj_port, "direction", &direction, &direction_len)) != CC_SUCCESS)
      cc_log_error("Failed to decode 'direction'");

    if (result == CC_SUCCESS && (result = cc_coder_get_value_from_map(obj_port, "properties", &obj_properties)) != CC_SUCCESS)
        cc_log_error("Failed to get 'properties'");

    if (result == CC_SUCCESS && (result = cc_coder_decode_uint_from_map(obj_properties, "nbr_peers", &nbr_peers)) != CC_SUCCESS)
      cc_log_error("Failed to get 'nbr_peers'");

    if (result == CC_SUCCESS && (result = cc_platform_mem_alloc((void **)&port, sizeof(cc_port_t))) != CC_SUCCESS)
      cc_log_error("Failed to allocate memory");

    if (result == CC_SUCCESS) {
      memset(port, 0, sizeof(cc_port_t));

      cc_gen_uuid(port->id, "PORT_");
      strncpy(port->name, name, name_len);
      port->name[name_len] = '\0';
      if (direction_len == 3 && strncmp(direction, "out", 3) == 0)
        port->direction = CC_PORT_DIRECTION_OUT;
      else
        port->direction = CC_PORT_DIRECTION_IN;
      port->state = CC_PORT_DISCONNECTED;
      port->actor = actor;
      port->peer_port = NULL;
      port->tunnel = NULL;
      port->fifo = cc_fifo_init_empty();
      if (port->fifo ==  NULL) {
        cc_log_error("Failed to init fifo");
        result = CC_FAIL;
      }

      if (port->direction == CC_PORT_DIRECTION_IN) {
        if (cc_list_add(&actor->in_ports, port->id, (void *)port, sizeof(cc_port_t)) == NULL) {
          cc_log_error("Failed to add port");
          result = CC_FAIL;
        }
      } else {
        if (cc_list_add(&actor->out_ports, port->id, (void *)port, sizeof(cc_port_t)) == NULL) {
          cc_log_error("Failed to add port");
          result = CC_FAIL;
        }
      }

      if (result != CC_SUCCESS && port != NULL)
        cc_port_free(node, port, false);
    }
  }

  return result;
}

static cc_result_t cc_app_manager_add_actor_args(cc_actor_t *actor, char *obj_arg)
{
  char *name = NULL, *arg = NULL;
  uint32_t name_len = 0, size = 0;

  if (cc_coder_decode_str(obj_arg, &name, &name_len) != CC_SUCCESS) {
    cc_log_error("Failed to decode key");
    return CC_FAIL;
  }

  cc_coder_decode_map_next(&obj_arg);

  size = cc_coder_get_size_of_value(obj_arg);
  if (cc_platform_mem_alloc((void **)&arg, size) != CC_SUCCESS) {
    cc_log_error("Failed to alllocate memory");
    return CC_FAIL;
  }
  memcpy(arg, obj_arg, size);

  if (cc_list_add_n(&actor->managed_attributes, name, name_len, arg, size) == NULL) {
    cc_log_error("Failed to add attribute");
    cc_platform_mem_free((void *)arg);
    return CC_FAIL;
  }

  return CC_SUCCESS;
}

static cc_actor_t *cc_app_manager_create_actor(cc_node_t *node, char *obj_actor)
{
  cc_actor_t *actor = NULL;
  char *actor_name = NULL, *actor_type = NULL, *args = NULL;
  uint32_t actor_name_len = 0, actor_type_len = 0, args_count = 0;
  cc_result_t result = CC_SUCCESS;
  int i = 0;

  if (cc_coder_decode_str(obj_actor, &actor_name, &actor_name_len) != CC_SUCCESS) {
    cc_log_error("Failed to decode key");
    result = CC_FAIL;
  }

  cc_coder_decode_map_next(&obj_actor);

  if (result == CC_SUCCESS && (result = cc_coder_decode_string_from_map(obj_actor, "actor_type", &actor_type, &actor_type_len)) != CC_SUCCESS)
    cc_log_error("Failed to decode 'actor_type'");

  if (result == CC_SUCCESS && (result = cc_coder_get_value_from_map(obj_actor, "args", &args)) != CC_SUCCESS)
    cc_log_error("Failed to get 'args'");

  cc_coder_decode_map_next(&obj_actor);

  if (result == CC_SUCCESS)
    actor = cc_actor_create_from_type(node, actor_type, actor_type_len);

  if (actor == NULL)
    result = CC_FAIL;

  if (result == CC_SUCCESS && (result = cc_platform_mem_alloc((void **)&actor->name, actor_name_len + 1)) != CC_SUCCESS)
    cc_log_error("Failed to alllocate memory");

  if (result == CC_SUCCESS) {
    strncpy(actor->name, actor_name, actor_name_len);
    actor->name[actor_name_len] = '\0';
  }

  if (result == CC_SUCCESS && (result = cc_platform_mem_alloc((void **)&actor->id, CC_UUID_BUFFER_SIZE)) != CC_SUCCESS)
    cc_log_error("Failed to alllocate memory");
  else {
    memset(actor->id, 0, CC_UUID_BUFFER_SIZE);
    cc_gen_uuid(actor->id, "ACTOR_");
  }

  if (result == CC_SUCCESS && cc_list_add(&node->actors, actor->id, (void *)actor, sizeof(cc_actor_t)) == NULL) {
    cc_log_error("Failed to add actor");
    result = CC_FAIL;
  }

  // add args
  args_count = cc_coder_decode_map(&args);
  for (i = 0; i < args_count && result == CC_SUCCESS; i++) {
    result = cc_app_manager_add_actor_args(actor, args);
    if (result != CC_SUCCESS) {
      cc_log_error("Failed to add actor args");
      break;
    }
    cc_coder_decode_map_next(&args);
    cc_coder_decode_map_next(&args);
  }

  if (result != CC_SUCCESS) {
    cc_platform_mem_free((void *)actor);
    actor = NULL;
  }

  return actor;
}

cc_result_t cc_app_manager_load_script(cc_node_t *node, const char *script)
{
  cc_result_t result = CC_SUCCESS;
  char *file_buffer = NULL, *buffer = NULL, *script_name = NULL;
  char *obj_actors = NULL, *obj_port_properties = NULL, *obj_connections = NULL, *obj_port_property = NULL;
  size_t size;
  uint32_t i = 0, actor_count = 0, script_name_len = 0;
  bool valid = false;
  cc_actor_t *actor = NULL;
  cc_port_t *port = NULL;
  cc_list_t *actors = NULL, *ports = NULL;
  char port_fullname[100];
  char *obj_port_connections = NULL;

  if ((result = cc_platform_file_read(script, &file_buffer, &size)) != CC_SUCCESS) {
    cc_log_error("Failed to open '%s'", script);
    return result;
  }

  buffer = file_buffer;

  if (result == CC_SUCCESS && (result = cc_coder_decode_string_from_map(buffer, "name", &script_name, &script_name_len)) != CC_SUCCESS)
    cc_log_error("Failed to decode 'name'");

  if (result == CC_SUCCESS && (result = cc_coder_decode_bool_from_map(buffer, "valid", &valid)) != CC_SUCCESS)
    cc_log_error("Failed to decode 'valid'");

  if (!valid) {
    cc_log_error("Script is not valid");
    result = CC_FAIL;
  }

  if (result == CC_SUCCESS && (result = cc_coder_get_value_from_map(buffer, "actors", &obj_actors)) != CC_SUCCESS)
    cc_log_error("Failed to get 'actors'");

  if (result == CC_SUCCESS && (result = cc_coder_get_value_from_map(buffer, "port_properties", &obj_port_properties)) != CC_SUCCESS)
    cc_log_error("Failed to get 'port_properties'");

  if (result == CC_SUCCESS && (result = cc_coder_get_value_from_map(buffer, "connections", &obj_connections)) != CC_SUCCESS)
    cc_log_error("Failed to get 'connections'");

  // create actors
  actor_count = cc_coder_decode_map(&obj_actors);
  for (i = 0; i < actor_count && result == CC_SUCCESS; i++) {
    actor = cc_app_manager_create_actor(node, obj_actors);
    if (actor == NULL) {
      result = CC_FAIL;
      break;
    }

    // get port_properties and create ports
    if (result == CC_SUCCESS && (result = cc_coder_get_value_from_map(obj_port_properties, actor->name, &obj_port_property)) != CC_SUCCESS)
      cc_log_error("Failed to get port_properties for '%s'", actor->name);
    else {
      if ((result = cc_app_manager_create_ports(node, actor, obj_port_property)) != CC_SUCCESS)
        cc_log_error("Failed to add ports for '%s'", actor->name);
    }

    // init actor
    if (result == CC_SUCCESS && (result = actor->init(actor, actor->managed_attributes)) != CC_SUCCESS)
      cc_log_error("Failed to add managed attriubutes");

    if (actor->managed_attributes != NULL)
      cc_actor_free_attribute_list(actor->managed_attributes);

    cc_coder_decode_map_next(&obj_actors);
    cc_coder_decode_map_next(&obj_actors);
  }

  // connect ports
  if (result == CC_SUCCESS) {
    actors = node->actors;
    while (actors != NULL) {
      actor = (cc_actor_t *)actors->data;
      ports = actor->out_ports;
      while (ports != NULL) {
        port = (cc_port_t *)ports->data;
        memset(port_fullname, 0, 100);
        snprintf(port_fullname, strlen(actor->name) + strlen(port->name) + 2, "%s.%s", actor->name, port->name);
        if (cc_coder_get_value_from_map(obj_connections, port_fullname, &obj_port_connections) == CC_SUCCESS) {
          result = cc_app_manager_setup_port_connections(node, port, obj_port_connections);
        }
        ports = ports->next;
      }
      actors = actors->next;
    }
  }

  cc_platform_mem_free(file_buffer);

	return result;
}
