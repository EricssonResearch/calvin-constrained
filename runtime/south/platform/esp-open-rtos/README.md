## esp-open-rtos

### Setup tool chain

Clone and setup the [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) tool chain. In the root of the calvin-constrained repo, edit Makefile-esp-rtos and set SDK_PATH to the root of the esp-open-rtos repository:

```
SDK_PATH = <ESP-OPEN-RTOS-PATH>
```

### Building and flash

Build and flash the binary with SDK, example with sleep enabled, the io.Light c actor included and support for Python actors enabled:

```
$ cd calvin-constrained
$ make -C libmpy esp8266
$ make flash -f Makefile-esp-rtos MPY=1 CONFIG="runtime/south/platform/esp-open-rtos/cc_config_esp8266_mpy.h" ESPPORT=/dev/ttyUSB0
```

### Configure WiFi

After flashing the firmware the runtime starts as a access point with the name and password "calvin-esp" and waits for configuration data on address 172.16.0.1 and port 80. The configuration data should be sent as a JSON dictionary with the attributes:
- attributes, runtime attributes
- proxy_uris, URI to proxy runtime
- ssid, WiFi SSID
- password, WiFi password

Example:

```
$ curl -X POST -d '{"attributes": {"indexed_public": {"node_name": {"name": "ESP8266"}}}, "uris": ["calvinip://192.168.0.41:5000", "ssdp"], "ssid": "<SSID>", "password": "<PASSWORD>"}' 172.16.0.1:80
```

The configuration data is written to flash and to set new attributes erase the flash with:

```
$ make erase_flash -f Makefile-esp-rtos ESPPORT=/dev/ttyUSB0
```

### Capture logs

```
$ minicom -D /dev/ttyUSB0 -b 115200
```
