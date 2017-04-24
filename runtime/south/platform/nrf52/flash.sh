#!/bin/sh
nrfjprog --eraseall -f nrf52
nrfjprog --program calvin_c.hex -f nrf52
nrfjprog --reset -f nrf52
