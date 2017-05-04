# calvin-constrained

calvin-constrained is a C based implementation of Calvin, https://github.com/EricssonResearch/calvin-base, targeting devices with constraints on memory, processing power and power consumption.

All functionality included in the standard Calvin implementation is not feasible to include on a constrained device and a standard Calvin runtime is used to offload functionality to:

- Route data to and from the constrained runtime
- Access the shared key/value store
- Deploy and redeploy applications

The constrained runtime finds a Calvin runtime to offload functionality by using a discovery mechanism or a predefined list of runtime URIs.

The state of the implementation is experimental and it goes without saying that it should not be used in production in its current state, but it does provide an excellent starting point for further experimentation.

## Setup

1.
Clone the constrained runtime repository:

```
git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git
```

Use the recursive flag to include the used third party modules.

2.
Follow the platform specific instruction in the runtime/south/platform/X/README.md.

## Runtime ports

Calvin-constrained is designed to be portable across different architectures and can execute on anything from bare metal to on top of an operating system.

The platform specific parts, such as initializing the platform and accessing hardware resources, are isolated from the core functionality. Runtime ports and instructions on how to create a new port are placed in the [runtime/south/platform](runtime/south/platform) folder.

## Transport clients

Transport clients handles the communication with the Calvin runtime acting as a proxy to offload functionality.

Implementations and instructions on how to create new transport clients are available in the [runtime/south/transport](runtime/south/transport) folder.

## Security

TLS can be enabled for all transport clients, the [runtime/south/crypto](runtime/south/crypto) folder contains instructions to enable it.

## Actors

The calvin-constrained runtime supports actors written:
- C, the [actors](actors) folder holds existing actors and instructions on how to write and add new actors
- Python, the [libmpy](libmpy) folder holds existing actors and instructions on how to add new actors

## Build configurations

- Serialization of the runtime state and recreation of the state is enabled with:

```
-DUSE_PERSISTENT_STORAGE -DPERSISTENT_STORAGE_CHECKPOINTING
```
