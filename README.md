# Calvin is no longer actively maintained.

Since the base version of Calvin has been retired, we unfortunately have to retire this repository as well. It will be kept in an archived state for the time being. You are welcome to fork, clone, and work on it, but it is unlikely there will be any further official releases and updates from our side.

It's been fun. Take care.

Cheers.

# calvin-constrained

calvin-constrained is a C based implementation of Calvin, https://github.com/EricssonResearch/calvin-base, targeting devices with constraints on memory, processing power and power consumption.

All functionality included in the standard Calvin implementation is not feasible to include on a constrained device and a standard Calvin runtime is used to offload functionality to:

- Route data to and from the constrained runtime
- Access the shared key/value store
- Deploy and redeploy applications

The constrained runtime finds a Calvin runtime to offload functionality by using a discovery mechanism or a predefined list of runtime URIs.

The state of the implementation is experimental and it goes without saying that it should not be used in production in its current state, but it does provide an excellent starting point for further experimentation.

## Setup

Clone the constrained runtime repository with the recursive flag to include used third party modules:

```
git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git
```

And follow the platform specific instructions in the runtime/south/platform/X/README.md to build and start the runtime.

## Runtime ports

Calvin-constrained is designed to be portable across different architectures and can execute on anything from bare metal to on top of an operating system.

The platform specific parts, such as initializing the platform and accessing hardware resources, are isolated from the core functionality. Runtime ports and instructions on how to create a new port are placed in the [runtime/south/platform](runtime/south/platform) folder.

## Runtime configuration

Runtime configuration parameters are listed in cc_config.h and can be overridden by creating a separate configuration file and by setting the CONFIG preprocessor define parsed by cc_config.h.

## Transport clients

Transport clients handles the communication with the Calvin runtime acting as a proxy to offload functionality. The constrained runtime can support and use multiple transport clients but only one is active handling the link to the runtime acting as a proxy for the constrained runtime.

Implementations and instructions on how to create new transport clients are available in the [runtime/south/transport](runtime/south/transport) folder.

## C actors

See [actors](actors) for instructions on how to write and add new actors.

## Python actors

See [libmpy](libmpy) for instructions on how to enable support for Python actors.

## Security

TLS can be enabled for all transport clients, the [runtime/south/crypto](runtime/south/crypto) folder contains instructions to enable it.
