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

rt1 = None  # Node with constrained rt connected
rt2 = None
request_handler = None
constrained_id = None
constrained_process = None
output_file = None

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
    global rt1
    global rt2
    global request_handler
    global constrained_id
    global constrained_process
    global output_file

    output_file = open("cc_stderr.log", "a")
    constrained_process = subprocess.Popen("exec ./calvin_c -a '{\"indexed_public\": {\"node_name\": {\"name\": \"constrained\"}}}' -p 'calvinip://127.0.0.1:5000 ssdp'", shell=True, stderr=output_file)

    request_handler = RequestHandler()
    rt1 = RT("http://127.0.0.1:5001")
    rt1.id = request_handler.get_node_id(rt1.control_uri)
    rt2 = RT("http://127.0.0.1:5003")
    rt2.id = request_handler.get_node_id(rt2.control_uri)

    # get constrained id
    for x in range(0, 10):
        peers = request_handler.get_nodes(rt1)
        for peer in peers:
            peer_info = request_handler.get_node(rt1, peer)
            if "constrained" in peer_info["attributes"]["indexed_public"][0]:
                constrained_id = peer
                return
        time.sleep(1)

    pytest.exit("Failed to get constrained runtime id")

def teardown_module(module):
    global constrained_process
    global output_file
    constrained_process.kill()
    output_file.close()

def testDataAndDestruction():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testDataAndDestruction"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "id": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """

    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':id'], constrained_id)

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # remove app
    request_handler.delete_application(rt1, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':id'])

def testDataAndDestructionRouted():
    assert rt2 is not None
    assert constrained_id is not None

    script_name = "testDataAndDestruction"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt2"}]},
                    "type": "+"
               }],
            "id": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt2"}]},
                    "type": "+"
                }]
        }
    }
    """

    resp = request_handler.deploy_application(rt2,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt2, resp['actor_map'][script_name + ':id'], constrained_id)

    # verify data
    wait_for_tokens(request_handler,
                    rt2,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt2,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # remove app
    request_handler.delete_application(rt2, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, rt2, resp['actor_map'][script_name + ':id'])

def testPortConnect():
    assert rt1 is not None
    assert rt2 is not None
    assert constrained_id is not None

    script_name = "testPortConnect"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "id": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """
    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    app_id = resp['application_id']
    identity_id = resp['actor_map'][script_name + ':id']
    snk_id = resp['actor_map'][script_name + ':snk']

    # verify placement
    assert verify_actor_placement(request_handler, rt1, identity_id, constrained_id)

    # wait for tokens
    wait_for_tokens(request_handler, rt1, resp['actor_map'][script_name + ':snk'], 5)
    actual = request_handler.report(rt1, resp['actor_map'][script_name + ':snk'])

    # migrate snk to rt2
    request_handler.migrate(rt1, snk_id, rt2.id)

    # verify placement
    assert verify_actor_placement(request_handler, rt2, snk_id, rt2.id)

    # verify tokens
    expected = len(actual) + 5
    wait_for_tokens(request_handler, rt2, resp['actor_map'][script_name + ':snk'], expected, 20)
    actual = request_handler.report(rt2, resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= expected

    # delete app
    request_handler.delete_application(rt1, app_id)

    # verify removal
    assert verify_actor_removal(request_handler, rt1, identity_id)

def testMigration():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testMigration"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "id": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """
    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':id'], constrained_id)

    # migrate back and verify placement
    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "id": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """
    request_handler.migrate_app_use_req(rt1,
                                        resp['application_id'],
                                        json.loads(deploy_info))


    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':id'], rt1.id)

    # delete application
    request_handler.delete_application(rt1, resp['application_id'])

    # verify removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':id'])

def testTemperatureActor():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testTemperatureActor"
    script = """
    temp : sensor.Temperature(frequency=1)
    snk : test.Sink(store_tokens=1, quiet=1)
    temp.centigrade > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "temp": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """
    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify actor placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':temp'], constrained_id)

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5
    assert all(x == 15.5 for x in actual)

    # delete application
    request_handler.delete_application(rt1, resp['application_id'])

    # verify removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':temp'])

def testLightActor():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testLed"
    script = """
    src : std.CountTimer(sleep=0.1)
    zero : std.Constantify(constant=0)
    led : io.Light()

    src.integer > zero.in
    zero.out > led.on
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "zero": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "led": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }]
        }
    }
    """
    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify actor placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':led'], constrained_id)

    # destroy application
    request_handler.delete_application(rt1, resp['application_id'])

    # verify removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':led'])

def testLocalConnections():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testLocalConnections"
    script = """
    src : std.CountTimer(sleep=0.1)
    id1 : std.Identity()
    id2 : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id1.token
    id1.token > id2.token
    id2.token > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "id1": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "id2": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """

    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':id1'], constrained_id)
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':id2'], constrained_id)

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # remove app
    request_handler.delete_application(rt1, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':id1'])
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':id2'])

def testSleepWithoutTimers():
    global constrained_process
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testSleepWithoutTimers"
    script = """
    src : std.CountTimer(sleep=4)
    id : std.Identity()
    snk : test.Sink(store_tokens=1, quiet=1)
    src.integer > id.token
    id.token > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "id": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """

    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':id'], constrained_id)

    # wait for constrained enterring sleep
    constrained_process.wait()

    assert constrained_process.poll() is not None

    constrained_process = subprocess.Popen("exec ./calvin_c", shell=True, stderr=output_file)

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 1, 10)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 1

    # remove app
    request_handler.delete_application(rt1, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':id'])

def testSleepWithTimer():
    global constrained_process
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testSleepWithTimer"
    script = """
    temp : sensor.Temperature(frequency=0.25)
    snk : test.Sink(store_tokens=1, quiet=1)
    temp.centigrade > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "temp": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """

    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':temp'], constrained_id)

    # wait for constrained enterring sleep
    constrained_process.wait()

    constrained_process = subprocess.Popen("exec ./calvin_c", shell=True, stderr=output_file)

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 2, 10)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 2

    # remove app
    request_handler.delete_application(rt1, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':temp'])

def testRegistryAttribute():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testRegistryAttribute"
    script = """
    tick : std.Trigger(data=null, tick=0.1)
    attr: context.RegistryAttribute(attribute="node_name.name")
    snk : test.Sink(store_tokens=1, quiet=1)

    tick.data > attr.trigger
    attr.value > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "tick": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "attr": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "constrained"}]},
                    "type": "+"
               }],
            "snk": [
                {
                    "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
                }]
        }
    }
    """

    resp = request_handler.deploy_application(rt1,
                                              script_name,
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    assert verify_actor_placement(request_handler, rt1, resp['actor_map'][script_name + ':attr'], constrained_id)

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5
    assert all(x == "constrained" for x in actual)

    # remove app
    request_handler.delete_application(rt1, resp['application_id'])

    # verify actor removal
    assert verify_actor_removal(request_handler, rt1, resp['actor_map'][script_name + ':snk'])
