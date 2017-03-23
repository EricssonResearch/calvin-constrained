# calvin-constrained

calvin-constrained is a C based implementation of https://github.com/EricssonResearch/calvin-base targeting constrained devices with limited memory and processing power. All functionality included in the standard calvin-base implementation is not feasible to include on a constrained device and a subset of the calvin-base functionality is implemented. A calvin-constrained runtime uses a calvin-base runtime as a proxy and delegates functionality for:

- routing of runtime to runtime communication
- accessing the global registry
- deployment and redeployment of applications

The state of the implementation is experimental and it goes without saying that it should not be used in production in its current state, but it does provide an excellent starting point for further experimentation.

## Clone

    git clone --recursive https://github.com/EricssonResearch/calvin-constrained.git

## Porting calvin-constrained to new environments

Calvin-constrained is designed to be portable across different architectures and can execute on anything from bare metal to on top of an operating system. Read [platform/README.md](platform/README.md) for instructions on how to port it to a new environment.

## Creating new transport clients

A transport client handles the runtime to runtime communication with the calvin-base runtime acting as a proxy, see [transport/README.md](transport/README.md) for instruction on how to create new transport clients.

## Actors

The calvin-constrained runtime supports both actors written in C and Python, see [actors/README.md](actors/README.md) for instructions on how to add new actors.

## Build configurations

Serialization to persistent storage of the runtime state and recreation of the state is enabled with:

    -DUSE_PERSISTENT_STORAGE
