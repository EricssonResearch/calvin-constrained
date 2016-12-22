# calvin-constrained

calvin-constrained is a C based implementation of https://github.com/EricssonResearch/calvin-base targeting constrained devices with limited memory and processing power. All functionality included in the standard calvin-base implementation is not feasible to include on a constrained device and a subset of the calvin-base functionality is implemented and a calvin-base runtime is required and used as a proxy and for offloading functionality from constrained runtimes.

The state of the implementation is experimental and it goes without saying that it should not be used in production in its current state, but it does provide an excellent starting point for further experimentation.

##Clone

    git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git

##Setup calvin-base

See https://github.com/EricssonResearch/calvin-base for information on how to install and start calvin-base.

##Python actors on calvin-constrained

Support for executing Python code is built on MicroPython, https://micropython.org/, setup MicroPython with:

    ./mp_setup.sh

Then use the makefiles ending with _mpy to include support for Python actors in calvin-constrained, MicroPython is included as a statically-linked library with actors in libmpy/modules/ included as frozen modules.

##Run tests

    cd calvin-constrained
    ./test/test.sh
    ./test/test.sh mpy # test Python actors

##*nix version

###Build

    cd calvin-constrained
    make -f platform/x86/Makefile

###Run

Start a calvin-base runtime:

    csruntime -n 127.0.0.1 --name constrained_proxy

Start calvin-constrained using service discovery to find a calvin-base runtime:

    ./calvin_c --name calvin-klein

Or start calvin-constrained specifying interface and port to connect to:

    ./calvin_c --name calvin-klein --proxy_iface 127.0.0.1 --proxy_port 5000

Verify that the connection is made by:

    curl http://127.0.0.1:5001/nodes
    ["d6b10ff3-593f-75a2-f07b-d1acc06961b6"]

    curl http://127.0.0.1:5001/node/d6b10ff3-593f-75a2-f07b-d1acc06961b6
    {"attributes": {"indexed_public": ["/node/attribute/node_name/////calvin-klein"], "public": {}}, "control_uri": null, "proxy": "acd1e55e-c609-4c00-80ac-a383eb5dbfc5", "authz_server": null, "uri": null}

Actors can now be migrated to and from the calvin-constrained runtime.

##Nordic Semiconductors NRF52 DK version

###Setup environment
Download the nRF5 IOT SDK (https://developer.nordicsemi.com/nRF5_IoT_SDK/nRF5_IoT_SDK_v0.9.x/) and nRF5x-Command-Line-Tools (https://www.nordicsemi.com/eng/nordic/download_resource/51505/18/58951603) and set:

        export NORDIC_SDK_PATH=<nrf5-iot-sdk_path>
        PATH=$PATH:<nrfjprog_path>
        PATH=$PATH:<mergehex_path>

Patch the nRF5 IOT SDK to include the MAC address of the connecting peer in the IPV6_MEDIUM_EVT_CONN_UP event:

        cd NORDIC_SDK_PATH
        patch components/iot/medium/ipv6_medium.h < <calvin-constrained_path>/platform/nrf52/sdk_patches/ipv6_medium.h.patch
        patch components/iot/medium/ipv6_medium_ble.c < <calvin-constrained_path>/platform/nrf52/sdk_patches/ipv6_medium_ble.c.patch

And download and install J-Link Software and Documentation Pack (https://www.segger.com/downloads/jlink)

###Build and flash

    cd calvin-constrained
    make -f platform/nrf52/pca10040/armgcc/Makefile
    ./platform/nrf52/flash.sh

###Run

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

###Capture logs

    sudo minicom -D /dev/ttyACM0 -b 1000000