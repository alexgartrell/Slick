#!/usr/bin/env python

import sys

import controller

OWN_PORT_START = 10000

FORWARDER_PORT = 12345
FORWARDER_CONTROL_PORT = 54321

def print_usage():
    print "Usage:"
    print "\t%s <own_ip> <forwarder_ip> <num_keys>" % sys.argv[0]
    sys.exit(1)

def main(argv):
    try:
        own_ip = argv[1]
        forwarder_addr = argv[2]
        num_keys = int(argv[3])
    except:
        print_usage()
    
    keys = range(num_keys)
    ports = map(lambda x: x + OWN_PORT_START, keys)


    c = controller.Controller((forwarder_addr, FORWARDER_CONTROL_PORT))
    c.open()
    c.release_servers()
    for k in keys:
        c.add_server(own_ip, OWN_PORT_START + k, k)
    c.close()
    
if __name__ == '__main__':
    main(sys.argv)
