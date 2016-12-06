# calvin-constrained

calvin-constrained is a C based implementation of https://github.com/EricssonResearch/calvin-base for constrained devices. All functionality included in the standard calvin-base implementation is not feasible to include on a constrained device with limited memory and processing power. calvin-constrained implements a subset of the calvin-base functionality and a calvin-base runtime is required acting as a proxy and for offloading functionality from the constrained runtime.

##Clone

    git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git

##Setup and start calvin-base acting as a proxy

See https://github.com/EricssonResearch/calvin-base for information on how to install and run calvin-base.
    
    csruntime --name calvin-ble-proxy -n 127.0.0.1

When started, the calvin-constrained runtime connects to a calvin-base runtime acting as a proxy and for offloading functionality from the constrained runtime.

##Run tests

    cd calvin-constrained
    ./test/test.sh

##*nix version

Build:

    cd calvin-constrained
    make -f platform/x86/Makefile

Start using service discovery to find a calvin-base runtime:

    ./calvin_c --name calvin-klein

Or start specifying calvin-base interface and port to connect to:

    ./calvin_c --name calvin-klein --proxy_iface <CALVINBASE_INTERFACE> --proxy_port <CALVINBASE_PORT>

The calvin-constrained runtime should be connected to the calvin-base runtime and actors can be migrated to it.

##Nordic Semiconductors NRF52 DK version

Required packages:

    sudo apt-get install gcc-arm-none-eabi radvd

Download the nRF5 IOT SDK (https://developer.nordicsemi.com/nRF5_IoT_SDK/nRF5_IoT_SDK_v0.9.x/) and nRF5x-Command-Line-Tools (https://www.nordicsemi.com/eng/nordic/download_resource/51505/18/58951603) and set paths:

        export NORDIC_SDK_PATH=<nrf5-iot-sdk_path>
        PATH=$PATH:<nrfjprog_path>
        PATH=$PATH:<mergehex_path>

Download and install J-Link Software and Documentation Pack (https://www.segger.com/downloads/jlink)

Edit radvd.conf to get a routable IPv6 address (see https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.iotsdk.v0.9.0%2Fiot_sdk_user_guides_radvd.html for more information):

    interface bt0
    {
        AdvSendAdvert on;
        prefix 2001:db8::/64
        {
            AdvOnLink off;
            AdvAutonomous on;
            AdvRouterAddr on;
        };
    };

Set IPv6 forwarding and start radvd service:

    sudo echo 1 > /proc/sys/net/ipv6/conf/all/forwarding
    sudo service radvd restart

Load and enable the bluetooth 6lowpan module:

    sudo modprobe bluetooth_6lowpan
    sudo echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable

Build and flash calvin-constrained:

    cd calvin-constrained
    make -f platform/nrf52/pca10040/armgcc/Makefile
    ./platform/nrf52/flash.sh

Get the NRF52 device address:

    hcitool lescan

Connect to the NRF52 device:

    sudo echo "connect <nrf_device_address> 1" > /sys/kernel/debug/bluetooth/6lowpan_control

Add prefix to the btX interface (if not done automatically):

    ifconfig bt0 add 2001:db8::1/64

The calvin-constrained runtime should now connect to the calvin-base runtime and actors can be migrated to it.

Capture logs:

    sudo minicom -D /dev/ttyACM0 -b 1000000