#!/bin/sh
nrfjprog --family NRF51 --program $NORDIC_SDK_PATH/components/softdevice/s1xx_iot/s1xx-iot-prototype2_softdevice.hex --chiperase --verify
nrfjprog --family NRF51 --program _build/nrf51422_xxac_s1xx_iot.hex --verify
nrfjprog --family NRF51 --reset
