## Nordic Semiconductors NRF52 DK

### Setup environment

Install gcc-arm-none-eabi:

        sudo apt-get install gcc-arm-none-eabi

Download the nRF5 IOT SDK (https://developer.nordicsemi.com/nRF5_IoT_SDK/nRF5_IoT_SDK_v0.9.x/) and nRF5x-Command-Line-Tools (https://www.nordicsemi.com/eng/nordic/download_resource/51505/18/58951603) and set:

        export NORDIC_SDK_PATH=<nrf5-iot-sdk_path>
        PATH=$PATH:<nrfjprog_path>
        PATH=$PATH:<mergehex_path>

Patch the nRF5 IOT SDK to include the MAC address of the connecting peer in the IPV6_MEDIUM_EVT_CONN_UP event:

        cd NORDIC_SDK_PATH
        patch components/iot/medium/ipv6_medium.h < <calvin-constrained_path>/platform/nrf52/sdk_patches/ipv6_medium.h.patch
        patch components/iot/medium/ipv6_medium_ble.c < <calvin-constrained_path>/platform/nrf52/sdk_patches/ipv6_medium_ble.c.patch

And download and install J-Link Software and Documentation Pack (https://www.segger.com/downloads/jlink)

### Build and flash

    cd calvin-constrained
    make -f runtime/south/platform/nrf52/pca10040/armgcc/Makefile
    ./platform/nrf52/flash.sh

### Run

Enable the bluetooth_6lowpan kernel module:

    sudo modprobe bluetooth_6lowpan
    sudo sh -c  "echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable"

On the same system, start a calvin-base runtime listening on port 5000:

    csruntime -n 192.168.0.110 -p 5000 -c 5001 --name constrained_proxy

The calvin-base runtime listens on all interfaces for incoming connections, the 192.168.0.110 address is the external address and used here to enable external access to the control API on port 5001.

Next, get the nRF52 device address:

    sudo hcitool lescan

Connect to the nRF52 device:

    sudo sh -c 'echo "connect 00:33:40:7D:C8:0D 1" > /sys/kernel/debug/bluetooth/6lowpan_control'

When a Bluetooth connection is established the calvin-constrained runtime will connect to the calvin-base runtime using the link local address of the btX interface, verify that the connection is established:

    curl http://192.168.0.110:5001/nodes
    ["5ef5de32-e0d9-6ee6-8bab-a7f1d7f72c4a"]

    curl http://192.168.0.110:5001/node/5ef5de32-e0d9-6ee6-8bab-a7f1d7f72c4a
    {"attributes": {"indexed_public": ["/node/attribute/node_name/////constrained"], "public": {}}, "control_uri": null, "proxy": "3682cb26-0ae0-4965-952b-86d1e1b1bcbb", "authz_server": null, "uri": null}

Actors can now be migrated to and from the calvin-constrained runtime.

### Capture logs

    sudo minicom -D /dev/ttyACM0 -b 1000000
