## x86

### Build
Example with

- Parsing of command line options
- Sleep
- Socket transport
- The C std.Identity actor

enabled:

```
cd calvin-constrained
make -f runtime/south/platform/x86/Makefile CC_GETOPT_ENABLED=1 CC_DEEPSLEEP_ENABLED=1 CC_TRANSPORT_SOCKET=1 CC_ACTOR_IDENTITY=1
```

Same as above but with support for Python actors and without the std.Identity C actor:

```
cd calvin-constrained
make -f runtime/south/platform/x86/Makefile mpy CC_GETOPT_ENABLED=1 CC_DEEPSLEEP_ENABLED=1 CC_TRANSPORT_SOCKET=1
```

### Run

Start a calvin-base runtime:

```
csruntime -n 127.0.0.1 --name constrained_proxy
```

Start calvin-constrained specifying runtime attributes and proxy uris:

```
./calvin_c -a '{"indexed_public": {"node_name": {"name": "constrained"}}}' -p 'calvinip://127.0.0.1:5000 ssdp'
```

With the above uris, a SSDP discovery will be triggered to find a proxy if a connection can't be made or is lost to calvinip://127.0.0.1:5000".

Verify that the connection is made by:

```
curl http://127.0.0.1:5001/nodes
["d6b10ff3-593f-75a2-f07b-d1acc06961b6"]

curl http://127.0.0.1:5001/node/d6b10ff3-593f-75a2-f07b-d1acc06961b6
{"attributes": {"indexed_public": ["/node/attribute/node_name/////calvin-klein"], "public": {}}, "control_uri": null, "proxy": "acd1e55e-c609-4c00-80ac-a383eb5dbfc5", "authz_server": null, "uri": null}
```

Actors can now be migrated to and from the calvin-constrained runtime.
