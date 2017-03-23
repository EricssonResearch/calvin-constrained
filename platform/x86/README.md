## x86

### Build

    cd calvin-constrained
    make -f platform/x86/Makefile

### Run

Start a calvin-base runtime:

    csruntime -n 127.0.0.1 --name constrained_proxy

Start calvin-constrained specifying a name and a proxy uri:

    ./calvin_c -n constrained -p "calvinip://127.0.0.1:5000"

The uri can be a whitespace separated list of uris to be used when finding a calvin-base runtime to be used as a proxy. Passing "ssdp" as a uri or part of a list of uris will trigger a discovery using ssdp to find a calvin-base runtime. Example:

    ./calvin_c -n calvin-klein -p "calvinip://127.0.0.1:5000 ssdp"

With the above uris, the discovery will be triggered if a connection can't be made to calvinip://127.0.0.1:5000".

Verify that the connection is made by:

    curl http://127.0.0.1:5001/nodes
    ["d6b10ff3-593f-75a2-f07b-d1acc06961b6"]

    curl http://127.0.0.1:5001/node/d6b10ff3-593f-75a2-f07b-d1acc06961b6
    {"attributes": {"indexed_public": ["/node/attribute/node_name/////calvin-klein"], "public": {}}, "control_uri": null, "proxy": "acd1e55e-c609-4c00-80ac-a383eb5dbfc5", "authz_server": null, "uri": null}

Actors can now be migrated to and from the calvin-constrained runtime.
