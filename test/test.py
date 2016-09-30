import time
import json
from calvin.requests.request_handler import RequestHandler, RT
from calvin.tests.helpers import wait_for_tokens

""" Test constrained runtime functionallity
"""

rt1 = None  # Node with constrained rt connected
request_handler = None
constrained_id = None


def setup_module(module):
    global rt1
    global request_handler
    global constrained_id

    request_handler = RequestHandler()

    for x in range(0, 10):
        tmp_rt = RT("http://127.0.0.1:5001")
        peers = request_handler.get_nodes(tmp_rt)
        if peers:
            constrained_id = peers[0]
            rt1 = tmp_rt
            rt1.id = request_handler.get_node_id(rt1.control_uri)
            return
        time.sleep(1)

    print "Runtime not found"


def testRemoteSrcWithDestruction():
    global rt1
    global request_handler

    assert rt1 is not None

    script = """
    src : std.CountTimer(sleep=0.1)
    snk : io.StandardOut(store_tokens=1, quiet=1)
    src.integer > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
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

    wait_for_tokens(request_handler,
                    rt1,
                    resp['actor_map']['testRemote:snk'], 5, 10)

    actual = request_handler.report(rt1,
                                    resp['actor_map']['testRemote:snk'])

    # verify data
    assert len(actual) >= 5

    # verify actor placement
    actor = request_handler.get_actor(rt1, resp['actor_map']['testRemote:src'])
    assert actor['node_id'] == constrained_id

    request_handler.delete_application(rt1, resp['application_id'])

    removed = False
    try:
        actor = request_handler.get_actor(rt1,
                                          resp['actor_map']['testRemote:src'])
    except:
        removed = True
    assert removed is True


def testRemoteSrcWithMigration():
    global rt1
    global request_handler

    assert rt1 is not None

    script = """
    src : std.CountTimer(sleep=0.1)
    snk : io.StandardOut(store_tokens=1, quiet=1)
    src.integer > snk.token
    """

    deploy_info = """
    {
        "requirements": {
            "src": [
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
    actor = request_handler.get_actor(rt1, resp['actor_map']['testRemote:src'])
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
    actor = request_handler.get_actor(rt1, resp['actor_map']['testRemote:src'])
    assert actor['node_id'] == rt1.id
