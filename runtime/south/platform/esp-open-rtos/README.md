## esp-open-rtos

### Setup tool chain

Clone and setup the [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) tool chain. In Makefile-esp-rtos, set the SDK_PATH to the root of the esp-open-rtos repository.

In runtime/south/platform/esp-open-rtos add a file named ssid_config.h and set:

```
#define WIFI_SSID "<YOUR_WIFI_SSID>"
#define WIFI_PASS "<YOUR_WIFI_PASSWORD>"
#define CALVIN_CB_URIS "<LIST_CALVIN_BASE_URIS>"
```

### Building and flash

Build and flash the binary with:

```
$ cd calvin-constrained
$ make flash -f MakefileESP8266 ESPPORT=/dev/ttyUSB0
```

If needed add permissions to /dev/ttyUSB0:

```
$ sudo chmod a+rw /dev/ttyUSB0
```

### Capture logs

```
$ sudo minicom -D /dev/ttyUSB0 -b 115200
```
