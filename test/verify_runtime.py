import sys
import time
from calvin.requests.request_handler import RequestHandler, RT

def get_node_id(rt):
    request_handler = RequestHandler()
    for x in range(0, 10):
        try:
            rt.id = request_handler.get_node_id(rt.control_uri)
            return True
        except:
            pass
        time.sleep(1)
    print "Failed to get node id from '%s'" % rt.control_uri
    return False

def main():
    if not get_node_id(RT(sys.argv[1])):
        print "Failed to get id"
        sys.exit(1)

if __name__ == '__main__':
    main()
