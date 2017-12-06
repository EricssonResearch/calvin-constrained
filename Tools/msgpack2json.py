import sys
import json
import umsgpack

f = open(sys.argv[1], 'r')
data = f.read()
unpacked = umsgpack.unpackb(data)
print "%s" % json.dumps(unpacked, indent=4, separators=(',', ': '))
f.close()
