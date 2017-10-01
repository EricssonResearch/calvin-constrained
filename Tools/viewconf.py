import json
import umsgpack

f = open('calvin.msgpack', 'r')
data = f.read()
unpacked = umsgpack.unpackb(data)
print "Data: %s\n" % json.dumps(unpacked, indent=4, separators=(',', ': '))
f.close()
