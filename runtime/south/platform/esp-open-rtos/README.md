## esp-open-rtos

### Setup tool chain

Clone and setup the [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) tool chain. In the root of the calvin-constrained repo, edit Makefile-esp-rtos and set SDK_PATH to the root of the esp-open-rtos repository:

```
SDK_PATH = <ESP-OPEN-RTOS-PATH>
```

### Building and flash

Build and flash the binary with:

```
$ cd calvin-constrained
$ make flash -f MakefileESP8266 ESPPORT=/dev/ttyUSB0
```

And if needed, add permissions to the ESPPORT with:

```
$ sudo chmod a+rw /dev/ttyUSB0
```

### Configure WiFi

After flashing the firmware the runtime starts as a access point, with the name and password "calvin-esp", and waits and listens for configuration data on address 172.16.0.1 and port 5003.

The configuration data should be sent as a JSON dictionary with the attributes:
- name, the runtime name
- proxy_uri, URI to proxy runtime
- ssid, WiFi SSID
- password, WiFi password

Example:

```
curl -X POST -d '{"name": "NodeMCU", "proxy_uri": "ssdp", "ssid": "<SSID>", "password": "<PASSWORD>"}' 172.16.0.1:5003
```

### Capture logs

```
$ sudo minicom -D /dev/ttyUSB0 -b 115200
```
