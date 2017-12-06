import sys
import json
import umsgpack

f = open(sys.argv[1], 'r')
data = f.read()
packed = umsgpack.packb(json.loads(data))
print "%s" % packed
f.close()
