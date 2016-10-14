import time
import json
from calvin.requests.request_handler import RequestHandler, RT
from calvin.utilities.nodecontrol import dispatch_node
from calvin.tests.helpers import wait_for_tokens

""" Test constrained runtime functionallity
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

    for x in range(0, 10):
        tmp_rt = RT("http://127.0.0.1:5001")
        peers = request_handler.get_nodes(tmp_rt)
        if peers:
            # constrained connected
            constrained_id = peers[0]
            rt1 = tmp_rt
            rt1.id = request_handler.get_node_id(rt1.control_uri)

            # dispatch 2nd runtime
            attr = {u'indexed_public': {u'node_name': {u'name': u'rt2'}}}
            rt2, _ = dispatch_node(['calvinip://127.0.0.1:5002'],
                                   'http://127.0.0.1:5003',
                                   attributes=attr)
            for y in range(0, 10):
                peers = request_handler.get_index(rt1, '/node/attribute/node_name')
                if len(peers['result']) is 3:
                    return
                time.sleep(1)
            return
        time.sleep(1)

    print "Runtime not found"


def teardown_module(module):
    request_handler.quit(rt2)


def testDataAndDestruction():
    assert rt1 is not None
    assert constrained_id is not None

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
                                              'testRemote',
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify actor placement
    actor = request_handler.get_actor(rt1, resp['actor_map']['testRemote:id'])
    assert actor['node_id'] == constrained_id

    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map']['testRemote:snk'], 5, 10)

    actual = request_handler.report(rt1,
                                    resp['actor_map']['testRemote:snk'])

    # verify data
    assert len(actual) >= 5

    request_handler.delete_application(rt1, resp['application_id'])

    removed = False
    try:
        actor = request_handler.get_actor(rt1,
                                          resp['actor_map']['testRemote:id'])
    except:
        removed = True
    assert removed is True


def testMigration():
    assert rt1 is not None
    assert constrained_id is not None

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
                                              'testRemote',
                                              script,
                                              deploy_info=json.loads(deploy_info))

    # verify placement
    actor = request_handler.get_actor(rt1, resp['actor_map']['testRemote:id'])
    assert actor['node_id'] == constrained_id

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
    actor = request_handler.get_actor(rt1, resp['actor_map']['testRemote:id'])
    assert actor['node_id'] == rt1.id

    request_handler.delete_application(rt1, resp['application_id'])


def testPortConnect():
    assert rt1 is not None
    assert rt2 is not None
    assert constrained_id is not None

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
                                              'testRemote',
                                              script,
                                              deploy_info=json.loads(deploy_info))

    app_id = resp['application_id']
    identity_id = resp['actor_map']['testRemote:id']
    snk_id = resp['actor_map']['testRemote:snk']

    # verify actor placement
    actor = request_handler.get_actor(rt1, identity_id)
    assert actor['node_id'] == constrained_id

    # migrate snk to rt2
    request_handler.migrate(rt1, snk_id, rt2.id)

    # Verify placement and port connection
    connected = False
    for x in range(0, 10):
        try:
            actor = request_handler.get_actor(rt2, snk_id)
            if actor['node_id'] == rt2.id:
                port = request_handler.get_port(rt2,
                                                snk_id,
                                                actor['inports'][0]['id'])
                connected = port['connected']
                if connected:
                    break
        except:
            pass
        time.sleep(1)
    assert connected

    request_handler.delete_application(rt1, app_id)
