import sys
import libconf

f = open(sys.argv[1])

conf = libconf.load(f)
print(conf)

conf['sys']['caches']['l3']['size'] = int(sys.argv[2]) * 1024 * 1024
conf['sys']['caches']['l3']['array']['ways'] = int(sys.argv[3])

f = open(sys.argv[1], 'w')
libconf.dump(conf, f)
