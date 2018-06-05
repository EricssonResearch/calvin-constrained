## x86

### Build and run

Build calvin-constrained:

```
make -f runtime/south/platform/x86/Makefile CONFIG="runtime/south/platform/x86/cc_config_x86_test.h"
```

Start a calvin-base runtime:

```
csruntime -n 127.0.0.1 --name constrained_proxy
```

Start calvin-constrained specifying runtime attributes and proxy uris:

```
./calvin_c -a '{"indexed_public": {"node_name": {"name": "constrained"}}}' -u '["calvinip://127.0.0.1:5000", "ssdp"]'
```

With the above uris, a SSDP discovery will be triggered to find a proxy if a connection can't be made or is lost to calvinip://127.0.0.1:5000".

Verify that the connection is made by:

```
curl http://127.0.0.1:5001/nodes
["d6b10ff3-593f-75a2-f07b-d1acc06961b6"]

curl http://127.0.0.1:5001/node/d6b10ff3-593f-75a2-f07b-d1acc06961b6
{"attributes": {"indexed_public": ["/node/attribute/node_name/////constrained"], "public": {}}, "control_uri": null, "proxy": "acd1e55e-c609-4c00-80ac-a383eb5dbfc5", "authz_server": null, "uri": null}
```

Actors can now be migrated to and from the calvin-constrained runtime.

### Build and run with COAP client support

Build calvin-constrained with COAP support:

```
make -f runtime/south/platform/x86/Makefile LIBCOAP=1 CONFIG="runtime/south/platform/x86/cc_config_x86_coap.h"
```

Start a calvin-constrained runtime with two actor types and two COAP capabilities:

```
./calvin_c -a '{"indexed_public": {"node_name": {"name": "constrained"}}}' -u '["calvinip://127.0.0.1:5000", "ssdp"]' -p '{"coap": {"actors": {"sensor.Temperature": "io.temperature", "sensor.Distance": "io.distance"}, "capabilities": {"io.temperature": "coap://127.0.0.1:57436/3303/0/5700", "io.distance": "coap://127.0.0.1:57436/3303/0/5700"}}}'
```

Actors are specified in the form: "actors": {"ACTOR_TYPE": "REQUIRED_CAPABILITY"} and capabilities are specified in the form: "capabilities": {"CAPABILITY": "COAP_RESOURCE"}. The runtime will observe the COAP resource and produce tokens when COAP data is available.

Example calvin script using sensor.Temperature:

```
temp : sensor.Temperature(period=1)
snk : io.Print()

temp.centigrade > snk.token

rule device: node_attr_match(node_name={"name": "constrained"})
apply temp: device
```
