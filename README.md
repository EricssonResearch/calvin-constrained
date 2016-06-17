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

Set IPv6 address for the NRF51 DK board to connect to:

    In main.c on line 13 set the IPv6 address of your calvin-base host

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

Get the NRF51 device address:

    hcitool lescan

Connect to the NRF51 device:

    echo "connect <nrf_device_address> 1" > /sys/kernel/debug/bluetooth/6lowpan_control

Start calvin-base:

    csruntime --name <name> -n <address> -p 5000

Press Button 1 on the NRF51 board to connect to the calvin-base runtime.

###*nix targets with BSD sockets

Build:

    make

Start calvin-base:

    PYTHONPATH=. python calvin/Tools/csruntime.py --name <name> -n <address> -p <port>

Start calvin-constrained:

    ./calvin_c -n <name> -i <calvin-base-address> -p <calvin-base-port>

##TODOs
* Test and write tests
* Support multiple port readers/writers
* Add connection listener and support multiple client connections
* Use persistent storage for node configuration
* Handle failures (malloc, transmit...)
* Add limited control API using the proxy
* Shrink the protocol, don't use strings when not needed (UUIDs, command types...) and remove redundant data
* Find proxy automatically
* Security is non-existent