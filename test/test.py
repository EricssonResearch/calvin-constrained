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

""" Test constrained runtime functionallity towards two base runtimes.
"""

request_handler = None
runtime1 = None
runtime2 = None
constrained_id = None
constrained_process = None
calvin_command = "exec ./calvin_c"
#calvin_command = "exec valgrind --leak-check=full ./calvin_c"

def verify_actor_placement(request_handler, rt, actor_id, rt_id):
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt, actor_id)
        except:
            retry = retry + 1
            time.sleep(1)
            continue
        if actor['node_id'] == rt_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(1)
    return migrated

def verify_actor_removal(request_handler, rt, actor_id):
    retry = 0
    removed = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt, actor_id)
        except:
            removed = True
            break
        retry = retry + 1
        time.sleep(1)
    return removed

def setup_module(module):
    global request_handler
    global runtime1
    global runtime2
    global constrained_id
    global constrained_process

    request_handler = RequestHandler()
    runtime1 = RT("http://127.0.0.1:5001")
    runtime1.id = request_handler.get_node_id(runtime1.control_uri)
    runtime2 = RT("http://127.0.0.1:5003")
    runtime2.id = request_handler.get_node_id(runtime2.control_uri)

    # start constrained
    constrained_process = subprocess.Popen(calvin_command + " -a '{\"indexed_public\": {\"node_name\": {\"organization\": \"com.ericsson\", \"purpose\": \"distributed-test\", \"group\": \"rest\", \"name\": \"constrained\"}}}' -u '[\"calvinip://127.0.0.1:5000\", \"ssdp\"]'", shell=True)
    for x in range(0, 10):
        peers = request_handler.get_nodes(runtime1)
        for peer in peers:
            peer_info = request_handler.get_node(runtime1, peer)
            if "constrained" in peer_info["attributes"]["indexed_public"][0]:
                constrained_id = peer
                return
        time.sleep(1)

    pytest.exit("Failed to get constrained runtimes")

def teardown_module(module):
    global constrained_process
    if constrained_process.poll() != 0:
        request_handler.abolish_proxy_peer(runtime1, constrained_id)
        time.sleep(1)
        if constrained_process.poll() != 0:
            constrained_process.kill()

def testDataAndDestruction():
    script_name = "testDataAndDestruction"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token
    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply id: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, resp['actor_map'][script_name + ':id'], constrained_id)

    # verify data
    wait_for_tokens(request_handler, runtime1, resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(runtime1, resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # remove app
    request_handler.delete_application(runtime1, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, runtime1, resp['actor_map'][script_name + ':id'])

def testDataAndDestructionRouted():
    script_name = "testDataAndDestruction"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token

    rule second: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "runtime2"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: second
    apply id: constrained
    """
    resp = request_handler.deploy_application(runtime2, script_name, script)

    # verify placement
    assert verify_actor_placement(request_handler, runtime2, resp['actor_map'][script_name + ':id'], constrained_id)

    # verify data
    wait_for_tokens(request_handler, runtime2, resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(runtime2, resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # remove app
    request_handler.delete_application(runtime2, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, runtime2, resp['actor_map'][script_name + ':id'])

def testPortConnect():
    script_name = "testPortConnect"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply id: constrained
    """

    resp = request_handler.deploy_application(runtime1, script_name, script)

    app_id = resp['application_id']
    identity_id = resp['actor_map'][script_name + ':id']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, identity_id, constrained_id)

    # wait for tokens
    wait_for_tokens(request_handler, runtime1, resp['actor_map'][script_name + ':snk'], 5)
    actual = request_handler.report(runtime1, resp['actor_map'][script_name + ':snk'])

    # migrate snk to rt2
    request_handler.migrate(runtime1, snk_id, runtime2.id)

    # verify placement
    assert verify_actor_placement(request_handler, runtime2, snk_id, runtime2.id)

    # verify tokens
    expected = len(actual) + 5
    wait_for_tokens(request_handler, runtime2, resp['actor_map'][script_name + ':snk'], expected, 20)
    actual = request_handler.report(runtime2, resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= expected

    # delete app
    request_handler.delete_application(runtime1, app_id)

    # verify removal
    assert verify_actor_removal(request_handler, runtime1, identity_id)

def testMigration():
    script_name = "testMigration"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply id: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, resp['actor_map'][script_name + ':id'], constrained_id)

    # verify data
    wait_for_tokens(request_handler, runtime1, resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(runtime1, resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # migrate id to rt2 and verify placement
    request_handler.migrate(runtime1, resp['actor_map'][script_name + ':id'], runtime2.id)

    # verify placement
    assert verify_actor_placement(request_handler, runtime2, resp['actor_map'][script_name + ':id'], runtime2.id)

    wait_for_tokens(request_handler, runtime1, resp['actor_map'][script_name + ':snk'], 10, 20)
    actual = request_handler.report(runtime1, resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 10

    # delete application
    request_handler.delete_application(runtime1, resp['application_id'])

def testTemperature():
    script_name = "testTemperature"
    script = """
    temp : sensor.Temperature(period=1)
    snk : test.Sink(store_tokens=1, quiet=1)
    temp.centigrade > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply snk: first
    apply temp: constrained
    """
    resp = request_handler.deploy_application(runtime1,
                                              script_name,
                                              script)
    temp_id = resp['actor_map'][script_name + ':temp']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify actor placement and data
    assert verify_actor_placement(request_handler, runtime1, temp_id, constrained_id)
    wait_for_tokens(request_handler, runtime1, snk_id, 2, 20)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 2

    # migrate temp to runtime1
    request_handler.migrate(runtime1, temp_id, runtime1.id)

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, temp_id, runtime1.id)

    # delete application
    request_handler.delete_application(runtime1, resp['application_id'])

def testTemperatureFromShadow():
    script_name = "testTemperatureFromShadow"
    script = """
    temp : sensor.Temperature(period=1)
    snk : test.Sink(store_tokens=1, quiet=1)
    temp.centigrade > snk.token

    rule second: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "runtime2"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply snk: second
    apply temp: constrained
    """
    resp = request_handler.deploy_application(runtime2,
                                              script_name,
                                              script)
    temp_id = resp['actor_map'][script_name + ':temp']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify actor placement and data
    assert verify_actor_placement(request_handler, runtime2, temp_id, constrained_id)
    wait_for_tokens(request_handler, runtime2, snk_id, 2, 20)
    actual = request_handler.report(runtime2, snk_id)
    assert len(actual) >= 2

    # migrate temp to runtime1
    request_handler.migrate(runtime2, temp_id, runtime1.id)

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, temp_id, runtime1.id)

    # delete application
    request_handler.delete_application(runtime2, resp['application_id'])

def testTriggeredTemperature():
    script_name = "testTriggeredTemperature"
    script = """
    src : std.CountTimer(sleep=0.1)
    temp : sensor.TriggeredTemperature()
    snk : test.Sink(store_tokens=1, quiet=1)

    src.integer > temp.trigger
    temp.centigrade > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply temp: constrained
    """
    resp = request_handler.deploy_application(runtime1,
                                              script_name,
                                              script)
    temp_id = resp['actor_map'][script_name + ':temp']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify actor placement and data
    assert verify_actor_placement(request_handler, runtime1, temp_id, constrained_id)
    wait_for_tokens(request_handler, runtime1, snk_id, 5, 20)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 5

    # migrate temp back to runtime1
    request_handler.migrate(runtime1, temp_id, runtime1.id)

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, temp_id, runtime1.id)

    # destroy application
    request_handler.delete_application(runtime1, resp['application_id'])

def testTriggeredTemperatureFromShadow():
    script_name = "testTriggeredTemperatureFromShadow"
    script = """
    src : std.CountTimer(sleep=0.1)
    temp : sensor.TriggeredTemperature()
    snk : test.Sink(store_tokens=1, quiet=1)

    src.integer > temp.trigger
    temp.centigrade > snk.token

    rule second: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "runtime2"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: second
    apply temp: constrained
    """
    resp = request_handler.deploy_application(runtime2,
                                              script_name,
                                              script)
    temp_id = resp['actor_map'][script_name + ':temp']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify actor placement and data
    assert verify_actor_placement(request_handler, runtime2, temp_id, constrained_id)
    wait_for_tokens(request_handler, runtime2, snk_id, 5, 20)
    actual = request_handler.report(runtime2, snk_id)
    assert len(actual) >= 5

    # migrate temp to runtime1
    request_handler.migrate(runtime2, temp_id, runtime1.id)

    # verify placement and data
    assert verify_actor_placement(request_handler, runtime1, temp_id, runtime1.id)

    # destroy application
    request_handler.delete_application(runtime2, resp['application_id'])

def testLight():
    script_name = "testLight"
    script = """
    src : std.CountTimer(sleep=0.1)
    seq : std.ConstSequencer(sequence=[0, 1])
    led : io.Light()

    src.integer > seq.in
    seq.out > led.on

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, seq: first
    apply led: constrained
    """
    resp = request_handler.deploy_application(runtime1,
                                              script_name,
                                              script)
    led_id = resp['actor_map'][script_name + ':led']

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime1, led_id, constrained_id)

    time.sleep(1)

    # migrate led to runtime1
    request_handler.migrate(runtime1, led_id, runtime1.id)

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime1, led_id, runtime1.id)

    # destroy application
    request_handler.delete_application(runtime1, resp['application_id'])

def testLightFromShadow():
    script_name = "testLightFromShadow"
    script = """
    src : std.CountTimer(sleep=0.1)
    seq : std.ConstSequencer(sequence=[0, 1])
    led : io.Light()

    src.integer > seq.in
    seq.out > led.on

    rule second: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "runtime2"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, seq: second
    apply led: constrained
    """
    resp = request_handler.deploy_application(runtime2,
                                              script_name,
                                              script)
    led_id = resp['actor_map'][script_name + ':led']

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime2, led_id, constrained_id)

    time.sleep(1)

    # migrate light to runtime1
    request_handler.migrate(runtime2, led_id, runtime1.id)

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime1, led_id, runtime1.id)

    # destroy application
    request_handler.delete_application(runtime2, resp['application_id'])

def testButton():
    script_name = "testButton"
    script = """
    button : io.Button(text="Ok")
    snk : test.Sink(store_tokens=1, quiet=1)

    button.state > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply snk: first
    apply button: constrained
    """
    resp = request_handler.deploy_application(runtime1,
                                              script_name,
                                              script)
    button_id = resp['actor_map'][script_name + ':button']

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime1, button_id, constrained_id)

    # migrate button to runtime1
    request_handler.migrate(runtime1, button_id, runtime1.id)

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime1, button_id, runtime1.id)

    # destroy application
    request_handler.delete_application(runtime1, resp['application_id'])

def testButtonFromShadow():
    script_name = "testButtonFromShadow"
    script = """
    button : io.Button(text="Ok")
    snk : test.Sink(store_tokens=1, quiet=1)

    button.state > snk.token

    rule second: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "runtime2"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply snk: second
    apply button: constrained
    """
    resp = request_handler.deploy_application(runtime2, script_name, script)
    button_id = resp['actor_map'][script_name + ':button']

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime2, button_id, constrained_id)

    # migrate button to runtime1
    request_handler.migrate(runtime2, button_id, runtime1.id)

    # verify actor placement
    assert verify_actor_placement(request_handler, runtime1, button_id, runtime1.id)

    # destroy application
    request_handler.delete_application(runtime2, resp['application_id'])

def testLocalConnections():
    script_name = "testLocalConnections"
    script = """
    src : std.CountTimer(sleep=0.1)
    id1 : std.Identity()
    id2 : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id1.token
    id1.token > id2.token
    id2.token > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply id1, id2: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)
    id1_id = resp['actor_map'][script_name + ':id1']
    id2_id = resp['actor_map'][script_name + ':id2']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, id1_id, constrained_id)
    assert verify_actor_placement(request_handler, runtime1, id2_id, constrained_id)

    # verify data
    wait_for_tokens(request_handler, runtime1, snk_id, 5, 20)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 5

    # remove app
    request_handler.delete_application(runtime1, resp['application_id'])

def testSleepWithoutTimers():
    global constrained_process
    script_name = "testSleepWithoutTimers"
    script = """
    src : std.CountTimer(sleep=4)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply id: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)
    id_id = resp['actor_map'][script_name + ':id']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, id_id, constrained_id)

    # wait for constrained enterring sleep
    constrained_process.wait()

    assert constrained_process.poll() is not None

    constrained_process = subprocess.Popen(calvin_command, shell=True)

    # verify data
    wait_for_tokens(request_handler, runtime1, snk_id, 1, 10)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 1

    # remove app
    request_handler.delete_application(runtime1, resp['application_id'])

def testSleepWithTimer():
    global constrained_process
    script_name = "testSleepWithTimer"
    script = """
    temp : sensor.Temperature(period=4)
    snk : test.Sink(store_tokens=1, quiet=1)
    temp.centigrade > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply snk: first
    apply temp: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)
    snk_id = resp['actor_map'][script_name + ':snk']
    temp_id = resp['actor_map'][script_name + ':temp']

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, temp_id, constrained_id)

    # wait for constrained enterring sleep
    constrained_process.wait()

    constrained_process = subprocess.Popen(calvin_command, shell=True)

    # verify data
    wait_for_tokens(request_handler, runtime1, snk_id, 1, 10)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 1

    # wait for constrained enterring sleep
    constrained_process.wait()

    constrained_process = subprocess.Popen(calvin_command, shell=True)

    # verify data
    wait_for_tokens(request_handler, runtime1, snk_id, 2, 10)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 2

    # remove app
    request_handler.delete_application(runtime1, resp['application_id'])

def testRegistryAttribute():
    script_name = "testRegistryAttribute"
    script = """
    tick : std.Trigger(data=null, tick=1)
    attr: context.RegistryAttribute(attribute="node_name.name")
    snk : test.Sink(store_tokens=1, quiet=0.1)

    tick.data > attr.trigger
    attr.value > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply tick, snk: first
    apply attr: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)
    attr_id = resp['actor_map'][script_name + ':attr']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify placement
    assert verify_actor_placement(request_handler, runtime1, attr_id, constrained_id)

    # verify data
    wait_for_tokens(request_handler, runtime1, snk_id, 5, 20)
    actual = request_handler.report(runtime1, snk_id)
    assert len(actual) >= 5
    assert all(x == "constrained" for x in actual)

    # remove app
    request_handler.delete_application(runtime1, resp['application_id'])

def testAbolish():
    script_name = "testAbolish"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token

    rule first: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "first", "name": "runtime1"}])
    rule constrained: node_attr_match(index=["node_name", {"organization": "com.ericsson", "purpose": "distributed-test", "group": "rest", "name": "constrained"}])
    apply src, snk: first
    apply id: constrained
    """
    resp = request_handler.deploy_application(runtime1, script_name, script)
    id_id = resp['actor_map'][script_name + ':id']

    assert verify_actor_placement(request_handler, runtime1, id_id, constrained_id)

    request_handler.abolish_proxy_peer(runtime1, constrained_id)

    assert verify_actor_placement(request_handler, runtime1, id_id, runtime1.id)

    assert constrained_process.poll() == 0

    request_handler.delete_application(runtime1, resp['application_id'])
