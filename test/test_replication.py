# -*- coding: utf-8 -*-

# Copyright (c) 2016 Ericsson AB
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import pytest
import time
import json
import subprocess
import os
import signal
from calvin.requests.request_handler import RequestHandler, RT
from calvin.utilities.nodecontrol import dispatch_node
from calvin.tests.helpers import wait_for_tokens
from calvin.utilities import calvinlogger
from calvin.tests import helpers

""" Test constrained runtime functionallity towards two base runtimes.
"""

request_handler = None
runtime1 = None
#calvin_command = "exec ./calvin_c"
calvin_command = "exec valgrind --leak-check=full ./calvin_c"
_log = calvinlogger.get_logger(__name__)

def setup_module(module):
    global request_handler
    global runtime1

    request_handler = RequestHandler()
    runtime1 = RT("http://127.0.0.1:5001")
    runtime1.id = request_handler.get_node_id(runtime1.control_uri)

class TestConstrainedReplication(object):
    def teardown_method(self, method):
        try:
            if self.constrained_proc:
                for cp in self.constrained_proc:
                    cp.terminate()
        except:
            pass
        self.constrained_proc = None
        try:
            if self.app_id is not None:
                helpers.delete_app(request_handler, runtime1, self.app_id)
        except:
            pass
        self.app_id = None

    def testConstrainedDeviceReplication(self):
        script = """
            src   : std.CountTimer(sleep=0.1)
            ident   : std.Identity()
            snk   : test.Sink(store_tokens=1, quiet=1)
            src.integer(routing="round-robin")
            snk.token(routing="collect-tagged")
            src.integer > ident.token
            ident.token > snk.token
            rule manual: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest"}]) & device_scaling()
            rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first"}])
            apply ident: manual
            apply src, snk: first
        """

        #Start constrained
        try:
            # Remove state if exist
            os.remove("calvin.msgpack")
        except:
            pass
        from subprocess import Popen
        constrained_p = range(3)
        for i in range(3):
            constrained_p[i] = Popen(['./calvin_c', '-a',
            '{"indexed_public": {"node_name": {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained%d"}}}' % i,
            '-u', '["calvinip://127.0.0.1:5000"]'])
        self.constrained_proc = constrained_p
        time.sleep(1)

        constrained_id = range(3)
        constrained_data = range(3)
        for i in range(3):
            fails = 0
            while fails < 20:
                try:
                    constrained_id[i] = request_handler.get_index(runtime1, "/node/attribute/node_name/com.ericsson//distributed-test/rest/constrained%d"%i)['result'][0]
                    constrained_data[i] = request_handler.get_node(runtime1, constrained_id[i])
                    break
                except:
                    fails += 1
                    time.sleep(0.1)

        response = helpers.deploy_script(request_handler, "testScript", script, runtime1)
        self.app_id = response['application_id']

        src = response['actor_map']['testScript:src']
        ident = response['actor_map']['testScript:ident']
        snk = response['actor_map']['testScript:snk']
        ident_rt_id = response['placement'][ident][0]

        replication_data = request_handler.get_storage(runtime1, key="replicationdata-" + response['replication_map']['testScript:ident'])['result']

        # Make sure we have all the replicas
        replicas = []
        fails = 0
        while len(replicas) < 3 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.3)
        assert len(replicas) == 3

        # Make sure that all paths thru replicas are used
        keys = []
        fails = 0
        while len(keys) < 5 and fails < 20:
            time.sleep(0.2)
            actual = request_handler.report(runtime1, snk)
            keys = set([k.keys()[0] for k in actual])
            fails += 1
        print keys
        print sorted([k.values()[0] for k in actual])
        assert len(keys) == 4

        # abolish a constrained runtime not having the original actor
        c_id = constrained_id[:]
        try:
            c_id.remove(ident_rt_id)
        except:
            print "original actor not on constrained"
        request_handler.abolish_proxy_peer(runtime1, c_id[0])

        # Make sure that the replica is gone
        fails = 0
        while len(replicas) > 3 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.3)
        assert len(replicas) == 3

        self.app_id = None
        helpers.delete_app(request_handler, runtime1, response['application_id'])

        # Now all replicas should be gone
        fails = 0
        while len(replicas) > 0 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.1)
        assert not replicas

        # Delete the remaining constrained runtimes
        cc_id = constrained_id[:]
        cc_id.remove(c_id[0])
        for i in cc_id:
            request_handler.abolish_proxy_peer(runtime1, i)

    def testConstrainedShadowDeviceReplication(self):
        script = """
            src   : std.CountTimer(sleep=0.1)
            ident   : test.FakeShadow()
            snk   : test.Sink(store_tokens=1, quiet=1)
            src.integer(routing="round-robin")
            snk.token(routing="collect-tagged")
            src.integer > ident.token
            ident.token > snk.token
            rule manual: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest"}]) & device_scaling()
            rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first"}])
            apply ident: manual
            apply src, snk: first
        """

        #Start constrained
        try:
            # Remove state if exist
            os.remove("calvin.msgpack")
        except:
            pass
        from subprocess import Popen
        constrained_p = range(3)
        for i in range(3):
            constrained_p[i] = Popen(['./calvin_c', '-a',
            '{"indexed_public": {"node_name": {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained%d"}}}' % i,
            '-u', '["calvinip://127.0.0.1:5000"]'])
        self.constrained_proc = constrained_p
        time.sleep(1)

        constrained_id = range(3)
        constrained_data = range(3)
        for i in range(3):
            fails = 0
            while fails < 20:
                try:
                    constrained_id[i] = request_handler.get_index(runtime1, "/node/attribute/node_name/com.ericsson//distributed-test/rest/constrained%d"%i)['result'][0]
                    constrained_data[i] = request_handler.get_node(runtime1, constrained_id[i])
                    break
                except:
                    fails += 1
                    time.sleep(0.1)

        response = helpers.deploy_script(request_handler, "testScript", script, runtime1)
        self.app_id = response['application_id']

        src = response['actor_map']['testScript:src']
        ident = response['actor_map']['testScript:ident']
        snk = response['actor_map']['testScript:snk']
        ident_rt_id = response['placement'][ident][0]

        replication_data = request_handler.get_storage(runtime1, key="replicationdata-" + response['replication_map']['testScript:ident'])['result']
        leader_id = replication_data['leader_node_id']

        # Make sure we have all the replicas
        replicas = []
        fails = 0
        while len(replicas) < 2 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.3)
        assert len(replicas) == 2

        keys = []
        fails = 0
        while len(keys) < 5 and fails < 20:
            time.sleep(0.2)
            actual = request_handler.report(runtime1, snk)
            keys = set([k.keys()[0] for k in actual])
            fails += 1
        print keys
        print sorted([k.values()[0] for k in actual])
        assert len(keys) == 3

        # abolish a constrained runtime not having the original actor
        c_id = constrained_id[:]
        try:
            c_id.remove(ident_rt_id)
        except:
            print "original actor not on constrained"
        request_handler.abolish_proxy_peer(runtime1, c_id[0])

        # Make sure that the replica is gone
        fails = 0
        while len(replicas) > 1 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.3)
        assert len(replicas) == 1

        self.app_id = None
        helpers.delete_app(request_handler, runtime1, response['application_id'])

        # Now all replicas should be gone
        fails = 0
        while len(replicas) > 0 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.1)
        assert not replicas

        # Delete the remaining constrained runtimes
        cc_id = constrained_id[:]
        cc_id.remove(c_id[0])
        for i in cc_id:
            request_handler.abolish_proxy_peer(runtime1, i)

    def testConstrainedDidReplication(self):
        script = """
            src   : std.CountTimer(sleep=0.03)
            ident   : test.ReplicaIdentity()
            snk   : test.Sink(store_tokens=1, quiet=1)
            src.integer(routing="random")
            snk.token(routing="collect-tagged")
            src.integer > ident.token
            ident.token > snk.token
            rule manual: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest"}]) & device_scaling()
            rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first"}])
            apply ident: manual
            apply src, snk: first
        """

        #Start constrained
        try:
            # Remove state if exist
            os.remove("calvin.msgpack")
        except:
            pass
        from subprocess import Popen
        constrained_p = range(3)
        for i in range(3):
            constrained_p[i] = Popen(['./calvin_c', '-a',
            '{"indexed_public": {"node_name": {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained%d"}}}' % i,
            '-u', '["calvinip://127.0.0.1:5000"]'])
        self.constrained_proc = constrained_p
        time.sleep(1)

        constrained_id = range(3)
        constrained_data = range(3)
        for i in range(3):
            fails = 0
            while fails < 20:
                try:
                    constrained_id[i] = request_handler.get_index(runtime1, "/node/attribute/node_name/com.ericsson//distributed-test/rest/constrained%d"%i)['result'][0]
                    constrained_data[i] = request_handler.get_node(runtime1, constrained_id[i])
                    break
                except:
                    fails += 1
                    time.sleep(0.1)

        response = helpers.deploy_script(request_handler, "testScript", script, runtime1)
        self.app_id = response['application_id']

        src = response['actor_map']['testScript:src']
        ident = response['actor_map']['testScript:ident']
        snk = response['actor_map']['testScript:snk']
        ident_rt_id = response['placement'][ident][0]

        replication_data = request_handler.get_storage(runtime1, key="replicationdata-" + response['replication_map']['testScript:ident'])['result']
        leader_id = replication_data['leader_node_id']

        # Make sure we have all the replicas
        replicas = []
        fails = 0
        while len(replicas) < 3 and fails < 20:
            replicas = request_handler.get_index(runtime1, "replicas/actors/"+response['replication_map']['testScript:ident'], root_prefix_level=3)['result']
            fails += 1
            time.sleep(0.3)
        assert len(replicas) == 3

        keys = []
        fails = 0
        while len(keys) < 5 and fails < 20:
            time.sleep(0.2)
            actual = request_handler.report(runtime1, snk)
            keys = set([k.keys()[0] for k in actual])
            fails += 1
        print keys
        assert len(keys) == 4

        self.app_id = None
        helpers.delete_app(request_handler, runtime1, response['application_id'])

        # Delete the constrained runtimes
        for i in constrained_id:
            request_handler.abolish_proxy_peer(runtime1, i)
