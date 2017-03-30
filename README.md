# calvin-constrained

calvin-constrained is a C based implementation of https://github.com/EricssonResearch/calvin-base targeting constrained devices with limited memory and processing power. All functionality included in the standard calvin-base implementation is not feasible to include on a constrained device and a subset of the calvin-base functionality is implemented. A calvin-constrained runtime uses a calvin-base runtime as a proxy and delegates functionality for:

- routing of runtime to runtime communication
- accessing the global registry
- deployment and redeployment of applications

The state of the implementation is experimental and it goes without saying that it should not be used in production in its current state, but it does provide an excellent starting point for further experimentation.

## Clone

    git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git

## Runtime ports

Calvin-constrained is designed to be portable across different architectures and can execute on anything from bare metal to on top of an operating system. The platform specific parts, such as initializing the platform and accessing hardware resources, are isolated from the core functionality. Implementations and instructions are located in [platform](platform).

## Transport clients

A transport client handles the runtime to runtime communication with the calvin-base runtime acting as a proxy. Implementations and instructions on how to create new transport clients are located in [transport](transport).

## Actors

The calvin-constrained runtime supports actors written in C and Python, instructions and implementations are located in [actors](actors) and [libmpy](libmpy).

## Build configurations

Serialization to persistent storage of the runtime state and recreation of the state is enabled with:

    -DUSE_PERSISTENT_STORAGE -DPERSISTENT_STORAGE_CHECKPOINTING
