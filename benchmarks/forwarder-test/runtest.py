#!/usr/bin/env python

import sys

import controller
import sender
import receiver
import time

OWN_PORT_START = 10000
PAYLOADS = [32, 64, 128, 256, 512, 1024]
ITERS=8000

FORWARDER_PORT = 12345
FORWARDER_CONTROL_PORT = 54321

def print_usage():
    print "Usage:"
    print "\t%s <own_ip> <forwarder_ip> <num_keys> [data_file]" % sys.argv[0]
    sys.exit(1)

def main(argv):
    try:
        own_ip = argv[1]
        forwarder_addr = argv[2]
        num_keys = int(argv[3])
        data_file = open(argv[4], 'w') if len(argv) > 4 else sys.stdout
    except:
        print_usage()
    
    keys = range(num_keys)
    ports = map(lambda x: x + OWN_PORT_START, keys)

    r = receiver.Receiver(ports)
    r.start()

    c = controller.Controller((forwarder_addr, FORWARDER_CONTROL_PORT))
    c.open()
    c.release_servers()
    for k in keys:
        c.add_server(own_ip, OWN_PORT_START + k, k)
    c.close()

    for p in PAYLOADS:
        print "Running", p
        s = sender.Sender((forwarder_addr, FORWARDER_PORT), keys, p, ITERS, 10)
        s.start()
        s.join()
        print "Finished", p
        time.sleep(5)

    c = controller.Controller((forwarder_addr, FORWARDER_CONTROL_PORT))
    c.open()
    c.release_servers()
    c.close()

    trials = r.get_trials()
    if len(trials) != len(PAYLOADS):
        print "WEIRDNESS! len(trials) =", len(trials), "len(PAYLOADS) = ", 
        print len(PAYLOADS)

    for i, t in enumerate(trials):
        bps = receiver.process_trial(t)
        data_file.write("%d\t%.2f\t%.3f\n" % 
                        (PAYLOADS[i], bps, bps / (1000 * 1000)))

    if data_file != sys.stdout:
        data_file.close()

    r.stop()
    print "Goodbye"
    sys.exit(0)
    
if __name__ == '__main__':
    main(sys.argv)
