# calvin-constrained

calvin-constrained is a C based implementation of Calvin,  https://github.com/EricssonResearch/calvin-base, targeting devices with constraints on memory, processing power and power consumption.

All functionality included in the standard Calvin implementation is not feasible to include on a constrained device and a standard Calvin runtime is used to offload functionality to:

- Route data to and from the constrained runtime
- Access the shared key/value store
- Deploy and redeploy applications

The state of the implementation is experimental and it goes without saying that it should not be used in production in its current state, but it does provide an excellent starting point for further experimentation.

## Clone
```
git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git
```

## Runtime ports

Calvin-constrained is designed to be portable across different architectures and can execute on anything from bare metal to on top of an operating system.

The platform specific parts, such as initializing the platform and accessing hardware resources, are isolated from the core functionality. Runtime ports and instructions on how to create a new port are placed in the [platform](platform) folder.

## Transport clients

Transport clients handles the communication with the Calvin runtime acting as a proxy to offload functionality.

Implementations and instructions on how to create new transport clients are available in the [transport](transport) folder.

TLS can be enabled for all transport clients, the [crypto](crypto) folder contains instructions to enable it.

## Actors

The calvin-constrained runtime supports actors written in C and Python, with actors and instruction in the [actors](actors) and [libmpy](libmpy) folders.

## Build configurations

- Serialization of the runtime state and recreation of the state is enabled with:

```
-DUSE_PERSISTENT_STORAGE -DPERSISTENT_STORAGE_CHECKPOINTING
```
