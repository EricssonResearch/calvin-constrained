## Build on x86

### Without Python support:
```
make -f runtime/south/platform/x86/Makefile CONFIG="runtime/south/platform/x86/cc_config_x86.h"
```

### With Python support:
```
make -C libmpy
make -f runtime/south/platform/x86/Makefile CONFIG="calvin_scripts/cc_config_script.h" MPY=1
```

### With CoAP client support:
The CoAP client calvinsys uses libcoap for the CoAP functionality, follow the installation instructions at https://libcoap.net/doc/install.html to install the library.

And build the runtime with:
```
make -f runtime/south/platform/x86/Makefile LIBCOAP=1 CONFIG="runtime/south/platform/x86/cc_config_x86_coap.h"
```
If libcoap isn't available on your environment and pkg-config fails when building, manually set the directives in runtime/south/platform/x86/Makefile with:
```
ifeq ($(LIBCOAP),1)
CC_SRC_C += runtime/south/platform/x86/calvinsys/cc_libcoap_client.c
CC_CFLAGS += -DCC_USE_LIBCOAP_CLIENT=1
CC_CFLAGS += -I/usr/local/include/coap2
CC_LDFLAGS += -L/usr/local/lib -lcoap-2
#CC_CFLAGS += $(shell pkg-config --cflags-only-I libcoap-1)
#CC_LDFLAGS += $(shell pkg-config --libs-only-L libcoap-1)
#CC_LIBS += $(shell pkg-config --libs-only-l libcoap-1)
endif
```

## Run
### Distributed with calvin-base
1. Start a calvin-base runtime:
```
csruntime -n 127.0.0.1 --name base
```
2. Start a constrained runtime connecting to the base runtime:
```
./calvin_c -a '{"indexed_public": {"node_name": {"name": "constrained"}}}' -u '["calvinip://127.0.0.1:5000", "ssdp"]'
```
Actors can now be migrated to and from the calvin-constrained runtime.

### Standalone (requires a net.HTTPGet and io.Print actor available when building with Python support)
1. Compile a calvin script:
```
cscompiler calvin_scripts/http_get_test.calvin
```
2. Encode it to msgpack format:
```
python Tools/json2msgpack.py calvin_scripts/http_get_test.json > calvin_scripts/http_get_test.msgpack
```
3. Start the runtime with the http_get_test script:
```
./calvin_c -s calvin_scripts/http_get_test.msgpack -a '{"indexed_public": {"node_name": {"name": "calvin_klein"}}}'
```
The runtime should now print the response code from the http request.

### With CoAP client support
With CoAP enabled the runtime creates actors and capabilities to represent a CoAP resource based on the platform_data (-p) start argument. The platform_data argument should be a JSON formatted string in the form:

-p '{"coap": {"actors": {"<ACTOR_NAME>": "<REQUIRED_CAPABILITY>"}, "capabilities": {"<CAPABILITY_NAME>": "<COAP_RESOURCE>"}}}'

Example with two actors exposing a temperature and a humidity CoAP resources:

-p '{"coap": {"actors": {"sensor.Temperature": "io.temperature", "sensor.Humidity": "io.humidity"}, "capabilities": {"io.temperature": "coap://127.0.0.1:57436/3303/0/5700", "io.humidity": "coap://127.0.0.1:57436/3304/0/5700"}}}'

The runtime will observe the CoAP resource and produce tokens when CoAP data is available.

#### With CoAP client support and standalone:

1. Compile and encode the calvin_scripts/temp_test.calvin script:
```
cscompiler calvin_scripts/temp_test.calvin
python Tools/json2msgpack.py calvin_scripts/temp_test.json > calvin_scripts/temp_test.msgpack
```

2. Start the constrained runtime with the temp_test script reading from a Coap source with URI coap://127.0.0.1:55654/3303/0/5700 (change it according to your environment):
```
./calvin_c -a '{"indexed_public": {"node_name": {"name": "constrained"}}}' -p '{"coap": {"actors": {"sensor.Temperature": "io.temperature"}, "capabilities": {"io.temperature": "coap://127.0.0.1:55654/3303/0/5700"}}}' -s calvin_scripts/temp_test.msgpack
```

The runtime should now print the temperature readings.

#### With CoAP client support and distributed with only the CoAP actor on the constrained runtime:
1. Start a calvin-base runtime:
```
csruntime -n 127.0.0.1 --name base
```
2. Start a calvin-constrained runtime connecting to the base runtime:
```
./calvin_c -a '{"indexed_public": {"node_name": {"name": "constrained"}}}' -p '{"coap": {"actors": {"sensor.Temperature": "io.temperature"}, "capabilities": {"io.temperature": "coap://127.0.0.1:55654/3303/0/5700"}}}' -u '["calvinip://127.0.0.1:5000"]'
```
3. Deploy a calvin-script that migrates a sensor actor to the constrained runtime, example script:
```
temp : sensor.Temperature(period=1)
snk : io.Print()
temp.centigrade > snk.token
rule device: node_attr_match(node_name={"name": "constrained"})
apply temp: device
```

The base runtime should now print the temperature readings.
