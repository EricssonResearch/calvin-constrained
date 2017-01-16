import sys
import time
import json
from calvin.requests.request_handler import RequestHandler, RT

def check_registry(rt):
    request_handler = RequestHandler()
    for x in range(0, 10):
        try:
            peers = request_handler.get_index(rt, '/node/attribute/node_name')
            if 'result' in peers and len(peers['result']) is 2:
                return True
            time.sleep(1)
        except:
            print "Failed to get nodes from index for %s, retrying" % rt.control_uri
    return False

def get_node_id(rt):
    request_handler = RequestHandler()
    for x in range(0, 10):
        try:
            rt.id = request_handler.get_node_id(rt.control_uri)
            return True
        except:
            print "Failed to get node id, retrying"
        time.sleep(1)
    return False

def main():
    rt1 = RT("http://127.0.0.1:5001")
    if not get_node_id(rt1):
        print "Failed to get rt1 id"
        sys.exit(1)

    if not check_registry(rt1):
        print "rt1 registry is not updated"
        sys.exit(1)

    rt2 = RT("http://127.0.0.1:5003")
    if not get_node_id(rt2):
        print "Failed to get rt2 id"
        sys.exit(1)

    if not check_registry(rt2):
        print "rt2 registry is not updated"
        sys.exit(1)

if __name__ == '__main__':
    main()
