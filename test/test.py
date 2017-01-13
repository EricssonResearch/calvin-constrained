import pytest
import time
import json
from calvin.requests.request_handler import RequestHandler, RT
from calvin.utilities.nodecontrol import dispatch_node
from calvin.tests.helpers import wait_for_tokens

""" Test constrained runtime functionallity towards two base runtimes.
"""

rt1 = None  # Node with constrained rt connected
rt2 = None
request_handler = None
constrained_id = None


def setup_module(module):
    global rt1
    global rt2
    global request_handler
    global constrained_id

    request_handler = RequestHandler()
    rt1 = RT("http://127.0.0.1:5001")
    rt2 = RT("http://127.0.0.1:5003")
    
    for y in range(0, 10):
        peers = request_handler.get_index(rt1, '/node/attribute/node_name')
        if len(peers['result']) is 3:
            peers = request_handler.get_nodes(rt1)
            if peers:
                # constrained connected
                constrained_id = peers[0]
                rt1.id = request_handler.get_node_id(rt1.control_uri)
                rt2.id = request_handler.get_node_id(rt2.control_uri)
                return
        time.sleep(1)
    
    pytest.exit("Not all runtimes found")

def teardown_module(module):
    request_handler.quit(rt2)


def testDataAndDestruction():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testDataAndDestruction"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : io.StandardOut(store_tokens=1, quiet=1)
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

    # verify actor placement
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':id'])
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == constrained_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5

    # Verify actor removal
    request_handler.delete_application(rt1, resp['application_id'])
    retry = 0
    removed = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':id'])
        except:
            removed = True
            break
        retry = retry + 1
        time.sleep(1)
    assert removed is True


def testPortConnect():
    assert rt1 is not None
    assert rt2 is not None
    assert constrained_id is not None

    script_name = "testPortConnect"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : io.StandardOut(store_tokens=1, quiet=1)
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

    # verify actor placement
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, identity_id)
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == constrained_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    # migrate snk to rt2
    request_handler.migrate(rt1, snk_id, rt2.id)

    # Verify placement
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt2, snk_id)
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == rt2.id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    # Verify ports connected
    retry = 0
    connected = False
    while retry < 20:
        port = request_handler.get_port(rt2,
                                        snk_id,
                                        actor['inports'][0]['id'])
        connected = port['connected']
        if connected:
            break
        retry = retry + 1
        time.sleep(0.5)
    assert connected

    request_handler.delete_application(rt1, app_id)
    retry = 0
    removed = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':id'])
        except:
            removed = True
            break
        retry = retry + 1
        time.sleep(1)
    assert removed is True

def testMigration():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testMigration"
    script = """
    src : std.CountTimer(sleep=0.1)
    id : std.Identity()
    snk : io.StandardOut(store_tokens=1, quiet=1)
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
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':id'])
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == constrained_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

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

    # Migrate back and verify placement
    request_handler.migrate_app_use_req(rt1,
                                        resp['application_id'],
                                        json.loads(deploy_info))


    # verify placement
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':id'])
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == rt1.id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    request_handler.delete_application(rt1, resp['application_id'])

def testTemperatureActor():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testTemperatureActor"
    script = """
    src : std.CountTimer(sleep=0.1)
    temp : sensor.Temperature()
    snk : io.StandardOut(store_tokens=1, quiet=1)
    src.integer > temp.measure
    temp.centigrade > snk.token
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
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':temp'])
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == constrained_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    # verify data
    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map'][script_name + ':snk'], 5, 20)
    actual = request_handler.report(rt1,
                                    resp['actor_map'][script_name + ':snk'])
    assert len(actual) >= 5
    assert all(x == 15.5 for x in actual)

    # Delete application
    request_handler.delete_application(rt1, resp['application_id'])
    retry = 0
    removed = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':temp'])
        except:
            removed = True
            break
        retry = retry + 1
        time.sleep(1)
    assert removed is True

def testGPIOReader():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testGPIOReader"
    script = """
    gpio : io.GPIOReader(gpio_pin=13, edge="b", pull="d")
    snk : io.StandardOut(store_tokens=1, quiet=1)
    gpio.state > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "gpio": [
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
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':gpio'])
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == constrained_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    # Delete application
    request_handler.delete_application(rt1, resp['application_id'])
    retry = 0
    removed = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':gpio'])
        except:
            removed = True
            break
        retry = retry + 1
        time.sleep(1)
    assert removed is True

def testGPIOWriter():
    assert rt1 is not None
    assert constrained_id is not None

    script_name = "testGPIOWriterActor"
    script = """
    src : std.CountTimer(sleep=0.1)
    alt : std.Dealternate()
    zero : std.Constantify(constant=0)
    one : std.Constantify(constant=1)
    join : std.Join()
    gpio : io.GPIOWriter(gpio_pin=13)

    src.integer > alt.token
    alt.token_1 > zero.in
    alt.token_2 > one.in
    zero.out > join.token_1
    one.out > join.token_2
    join.token > gpio.state
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
            "alt": [
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
            "one": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "join": [
                {
                  "op": "node_attr_match",
                    "kwargs": {"index": ["node_name", {"name": "rt1"}]},
                    "type": "+"
               }],
            "gpio": [
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
    retry = 0
    migrated = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':gpio'])
        except:
            retry = retry + 1
            time.sleep(0.5)
            continue
        if actor['node_id'] == constrained_id:
            migrated = True
            break
        retry = retry + 1
        time.sleep(0.5)
    assert migrated

    # Destroy application
    request_handler.delete_application(rt1, resp['application_id'])
    retry = 0
    removed = False
    while retry < 20:
        try:
            actor = request_handler.get_actor(rt1, resp['actor_map'][script_name + ':gpio'])
        except:
            removed = True
            break
        retry = retry + 1
        time.sleep(1)
    assert removed is True
