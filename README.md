# calvin-constrained

## What is this?

calvin-constrained is a C based implementation of https://github.com/EricssonResearch/calvin-base for constrained devices. A subset of the calvin-base functionality is implemented and a calvin-base runtime is required for configuration and for storage access.

##TARGETS
###Nordic Semiconductors NRF51 DK with IPv6 over Bluetooth LE

Download the nRF51_IoT_SDK:

    https://developer.nordicsemi.com/nRF5_IoT_SDK/nRF51_IoT_SDK_v0.8.x/

Set environment variables:

    export NORDIC_SDK_PATH=/NR51_IOT_SDK_PATH/Nordic/nrf51
    PATH=$PATH:/NRFJPROG_PATH/nrfjprog/
    PATH=$PATH:/JLINK_PATH/JLink_Linux_V512_x86_64

Build:

    cd boards/pca10028/armgcc/
    make

Flash:

    ./flash.sh

Output and debug prints:

    sudo minicom --device /dev/ttyACM0  --b 115200

Load and enable the bluetooth 6lowpan module:

    modprobe bluetooth_6lowpan
    echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable

Start calvin-base:

    csruntime --name <name> -n <address> -p 5000 &

Get the NRF51 device address:

    hcitool lescan

Connect to the NRF51 device:

    echo "connect <nrf_device_address> 1" > /sys/kernel/debug/bluetooth/6lowpan_control

The calvin-constrained runtime on the NRF51 board connects to the link local address of the connected Bluetooth peer.

###*nix targets with BSD sockets

Build:

    make

Start calvin-base:

    PYTHONPATH=. python calvin/Tools/csruntime.py --name <name> -n <address> -p <port>

Start calvin-constrained:

    ./calvin_c

The calvin-constrained runtime discovers a calvin-base runtime and connects to it.

##Protocol
Sequence:

    +--------------------+                                              +-------------+
    | calvin_constrained |                                              | calvin_base |
    +--------------------+                                              +-------------+

    Connect:
                                           JOIN_REQUEST
                                    +------------------------->
                                           JOIN_REPLY
                                    <-------------------------+
    Create storage tunnel:
                                           TUNNEL_NEW (type: storage)
                                    +------------------------->
                                           REPLY
                                    <-------------------------+
    Configure node:
                                           PROXY_CONFIG
                                    +------------------------->
                                           REPLY
                                    <-------------------------+

    Create a link to calvin-constrained:
                                           ROUTE_REQUEST
                                    <-------------------------+
                                           REPLY
                                    +------------------------->

    Migrate actor to calvin-constrained:
                                           ACTOR_NEW
                                    <-------------------------+
    Create token tunnel:
                                           TUNNEL_NEW (type: token)
                                    +------------------------->
                                           REPLY
                                    <-------------------------+
    Store port(s):
                                           SET (key: port-<PORT_ID>)
                                    +------------------------->
                                           REPLY
                                    <-------------------------+
    Store actor:
                                           SET (key: actor-<ACTOR_ID>)
                                    +------------------------->
                                           REPLY
                                    <-------------------------+
                                           REPLY (ACTOR_NEW)
                                    +------------------------->
    Connect port:
                                           PORT_CONNECT
                                    +------------------------->
                                           REPLY
                                    <-------------------------+


    Transfer token:
                                           TOKEN
                                    <------------------------->
                                           TOKEN_REPLY
                                    <------------------------->


    Migrate actor back to calvin-base:
                                           ACTOR_MIGRATE
                                    <-------------------------+
    Disconnect port(s):
                                           PORT_DISCONNECT
                                    +------------------------->
                                           REPLY
                                    <-------------------------+
    Migrate actor:
                                           ACTOR_NEW
                                    +------------------------->
                                           REPLY (ACTOR_NEW)
                                    <-------------------------+
                                           REPLY (ACTOR_MIGRATE)
                                    +------------------------->

JOIN_REQUEST:

    {
      'cmd': 'JOIN_REQUEST',
      'id': '<NODE_ID>',                                # ID of node sending the request
      'sid': <MESSAGE_ID>,                              # Message id
      'serializers': ['<SERIALISER>']                   # Serializers: json/msgpack
    }

    {
      'cmd': 'JOIN_REPLY',
      'id': '<NODE_ID>',                                # ID of replying node
      'serializer': '<SERIALIZER>',                     # Selected serializer used for all subsequent commands
      'sid': '<MESSAGE_ID>'                             # sid from JOIN_REQUEST
    }

ROUTE_REQUEST:

    {
      'cmd': 'ROUTE_REQUEST',
      'msg_uuid': '<NODE_ID>',                          # Message id
      'from_rt_uuid': <NODE_ID>,                        # ID of sending node
      'to_rt_uuid': '<NODE_ID>',                        # ID of receiving node
      'dest_peer_id': '<NODE_ID>',                      # ID of destination node to which the link should be created
      'org_peer_id': '<NODE_ID>'                        # ID of node requesting the link
    }

    {
      'cmd': 'REPLY',
      'to_rt_uuid': '<NODE_ID>',                        # Destination node id
      'from_rt_uuid': '<NODE_ID>',                      # Source node id
      'msg_uuid': '<MESSAGE_ID>',                       # msg_uuid from ROUTE_REQUEST command
      'value':
      {
        'status': 200,
        'success_list': [200, 201, 202, 203, 204, 205, 206],
        'data':
        {
          'peer_id': '<NODE_ID>'                        # ID of node sending the reply
        }
      }
    }

TUNNEL_NEW:

    {
      'cmd': 'TUNNEL_NEW',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<MESSAGE_ID>',                       # Message id
      'tunnel_id': '<TUNNEL_ID>',                       # ID for new tunnel
      'policy': {},
      'type': '<TYPE>'                                  # Tunnel types: storage/token/control
    }

    {
      'cmd': 'REPLY',
      'to_rt_uuid': '<NODE_ID>',                        # Destination node id
      'from_rt_uuid': '<NODE_ID>',                      # Source node id
      'msg_uuid': '<MESSAGE_ID>',                       # msg_uuid from TUNNEL_NEW command
      'value':
      {
        'status': 200,
        'success_list': [200, 201, 202, 203, 204, 205, 206],
        'data':
        {
          'tunnel_id': '<TUNNEL_ID>'                    # Tunnel id from TUNNEL_NEW
        }
      }
    }

PROXY_CONFIG:

    {
      'cmd': 'PROXY_CONFIG',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<MESSAGE_ID>',                       # Message id
      'name': <NODE_NAME>,                              # Node name
      'vid': <VENDOR_ID>,                               # Vendor id, configured in calvin-base/calvin/utilities/proxyconfig.py
      'pid': <PRODUCT_ID>                               # Product id, configured in calvin-base/calvin/utilities/proxyconfig.py
    }

    {
      'cmd': 'REPLY',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<MESSAGE_ID>',                       # msg_uuid PROXY_CONFIG command
      'value':
      {
        'status': 200,
        'success_list': [200, 201, 202, 203, 204, 205, 206],
        'data': None
      }
    }

SET:

    {
      'cmd': 'TUNNEL_DATA',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'tunnel_id': '<TUNNEL_ID>'                        # ID of storage tunnel
      'value':
      {
        'cmd': 'SET',
        'key': '<KEY>',                                 # Keys: node-<NODE_ID>/actor-<ACTOR_ID>/port-<PORT_ID>
        'value': '<VALUE>',                             # Values: see 1., 2. and 3.
        'msg_uuid': '<MESSAGE_ID>'                      # Message id
      },
    }

    1. Node value:
    {
      "attributes":
      {
        "indexed_public": [],
        "public": {}
      },
      "control_uri": "",
      "uri": []
    }

    2. Actor value:
    {
      "is_shadow": <VALUE>,                             # Values: true/false
      "name": "<NAME>",                                 # Actor name
      "node_id": "<NODE_ID>",                           # Node id
      "type": "<ACTOR_TYPE>",                           # Actor type
      "inports": [{
                   "id": "<PORT_ID>",                   # Port id
                   "name": "<PORT_NAME>"                # Port name
                   }],
      "outports": [{
                   "id": "<PORT_ID>",                   # Port name
                   "name": "<PORT_NAME>"                # Port id
                   }]
    }

    3. Port value:
    {
      "properties":
      {
        "direction": "<DIRECTION>",                     # Direction: in/out
        "routing": "<ROUTING>",                         # Routing: default/fanout
        "nbr_peers": <NBR_PEERS>                        # Number of peers connected to this port
      },
      "name": "<PORT_NAME>",                            # Port name
      "node_id": "<NODE_ID>",                           # Node id
      "connected": <CONNECTED>,                         # Value: true/false
      "actor_id": "<ACTOR_ID>"                          # Actor id
    }

    {
      'cmd': 'TUNNEL_DATA',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'tunnel_id': '<TUNNEL_ID>',                       # Id of storage tunnel
      'value':
      {
        'cmd': 'REPLY',
        'value': <VALUE>,                               # Value: True/False
        'key': '<KEY>',                                 # Key from SET command
        'msg_uuid': '<MESSAGE_ID>'                      # Message id from SET command
      }
    }

ACTOR_NEW:

    {
      'cmd': 'ACTOR_NEW',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<NODE_ID>',                          # Message id
      'state':
      {
        'prev_connections':
        {
          'actor_id': '<ACTOR_ID>',                     # Actor id
          'actor_name': '<ACTOR_NAME>',                 # Actor name
          'inports':                                    # Inports and their connections
          {
            '<PORT_ID>': [('<PEER_ID>', '<PEER_PORT_ID>')]
          },
          'outports':                                   # Outports and their connections
          {
            '<PORT_ID>': [('<PEER_ID>', '<PEER_PORT_ID>')]
          }
        },
        'actor_type': '<ACTOR_TYPE>',                   # Actor type
        'actor_state':
        {
          'name': '<ACTOR_NAME>',                       # Actor name
          'inports': {<PORT>},                          # Port properties, see 1.
          'outports': {<PORT>},                         # Port properties, see 1.
          '_component_members': ['<ACTOR_ID>'],         # Component members
          '_managed': ['<ATTRIBUTE>''],                 # Managed attributes
          'id': '<ACTOR_ID>',                           # Actor id
          '_signature': '<SIGNATURE>',                  # Actor signature
          '_deployment_requirements': [],               # Deployment requirements
          'migration_info': None,
          'subject_attributes': None
        }
      }
    }

    1. Port properties:
    '<PORTNAME>':                                       # Port name
    {
      'queue':
      {
        'queuetype': 'fanout_fifo',
        'write_pos': <POSITION>,                        # Current write position in fifo/queue
        'readers': ['<PORT_ID>'],                       # Port readers
        'fifo': [{'data': <VALUE>, 'type': 'Token'}],   # Current fifo
        'N': <SIZE>,                                    # FIFO/queue size
        'tentative_read_pos': {'<READER>': POSITION},   # Tentative read positions
        'read_pos': {'<READER>': <POSITION>}            # Current read positions
      },
      'properties':
      {
        'nbr_peers': <NBR_PEERS>,                       # Number of peers connected to this port
        'direction': '<DIRECTION>',                     # Direction: in/out
        'routing': '<TYPE>'                             # Routing: default/fanout
      },
      'name': '<PORT_NAME>',
      'id': '<PORT_ID>'
    }

    {
      'cmd': 'REPLY',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<MESSAGE_ID>'                        # msg_uuid from ACTOR_NEW command
      'value':
      {
        'status': 200,
        'success_list': [200, 201, 202, 203, 204, 205, 206],
        'data': None
      }
    }

PORT_CONNECT:

    {
      'cmd': 'PORT_CONNECT',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<MESSAGE_ID>'                        # Message id
      'port_properties':
      {
        'nbr_peers': <NBR_PEERS>,                       # Number of peers connected to this port
        'direction': '<DIRECTION>',                     # Values: in/out
        'routing': '<TYPE>'                             # Values: default/fanout
      },
      'tunnel_id': '<TUNNEL_ID>',                       # Token tunnel id
      'port_id': '<PORT_ID>',                           # ID of source port
      'peer_port_id': '<PEER_PORT_ID>',                 # ID of destination port
      'peer_actor_id': None,
      'peer_port_name': None,
      'peer_port_properties': None
    }

    {
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'cmd': 'REPLY',
      'value':
      {
        'status': 200,
        'success_list': [200, 201, 202, 203, 204, 205, 206],
        'data':
        {
          'port_id': '<PORT_ID>'                        # peer_port_id from PORT_CONNECT
        }
      },
      'msg_uuid': '<MESSAGE_ID>'                        # msg_uuid from PORT_CONNECT command
    }

TOKEN:

    {
      'cmd': 'TUNNEL_DATA',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'tunnel_id': '<TUNNEL_ID>',                       # Token tunnel id
      'value':
      {
        'cmd': 'TOKEN',
        'sequencenbr': <SEQ_NBR>,                       # Sequence number
        'token':
        {
          'data': <DATA>,                               # Token data
          'type': '<TYPE>'                              # Values: Token/ExceptionToken/EOSToken
        },
        'port_id': '<PORT_ID>',                         # ID of source port
        'peer_port_id': '<PEER_PORT_ID>'                # ID of destination port
      }
    }

    {
      'cmd': 'TUNNEL_DATA',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'tunnel_id': '<TUNNEL_ID>'                        # Token tunnel id
      'value':
      {
        'cmd': 'TOKEN_REPLY',
        'sequencenbr': <SEQ_NBR>,                       # Sequence number
        'port_id': '<PORT_ID>',                         # port_id from TOKEN command
        'peer_port_id': '<PEER_PORT_ID>',               # peer_port_id from TOKEN command
        'value': '<VALUE>'                              # Values: ACK/NACK
      }
    }

PORT_DISCONNECT:

    {
      'cmd': 'PORT_DISCONNECT',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'tunnel_id': '<TUNNEL_ID>',                       # Token tunnel id
      'msg_uuid': '<MESSAGE_ID>',                       # Message id
      'port_id': '<PORT_ID>',                           # ID of source port
      'peer_port_id': '<PEER_PORT_ID>',                 # ID of destination port
      'peer_port_name': None,
      'peer_actor_id': None,
      'peer_port_dir': None,
    }

    {
      'cmd': 'REPLY',
      'to_rt_uuid': '<NODE_ID>',                        # ID of destination node
      'from_rt_uuid': '<NODE_ID>',                      # ID of source node
      'msg_uuid': '<MESSAGE_ID>'                        # msg_uuid from PORT_DISCONNECT
      'value':
      {
        'status': 200,
        'success_list': [200, 201, 202, 203, 204, 205, 206],
        'data': None
      },
    }

##TODOs
* Test and write tests
* Support multiple port readers/writers
* Add connection listener and support multiple client connections
* Use persistent storage for node configuration
* Handle failures (malloc, transmit...)
* Add limited control API using the proxy
* Shrink the protocol, don't use strings when not needed (UUIDs, command types...) and remove redundant data
* Security is non-existent