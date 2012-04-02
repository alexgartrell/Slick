#!/usr/bin/env python

import sys

import socket
import threading
import time

def process_trial(trial):
    my_trial = trial[:] if len(trial) < 3 else trial[1:-1]
    total = reduce(lambda tot, t: tot + t[2], my_trial, 0.0)
    return total * 8 / (my_trial[-1][1] - my_trial[0][0])

class ReceiverWorker(threading.Thread):
    def __init__(self, receiver, port):
        threading.Thread.__init__(self)
        self.receiver = receiver
        self.port = port
        self.killed = threading.Event()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.settimeout(1.0)
        self.sock.bind(('', port))
        self.sock.listen(10)

    def run(self):
        while not self.killed.is_set():
            try:
                (s, addr) = self.sock.accept()
                s.settimeout(1.0)
                while True:
                    try:
                        amt = len(s.recv(16000))
                        if amt == 0:
                            break
                        self.receiver.update(amt)
                    except:
                        pass
                s.close()
            except socket.timeout:
                continue
        

    def stop(self):
        self.killed.set()
        self.sock.close()

class Receiver(threading.Thread):
    def __init__(self, ports,  display_results=False):
        assert len(ports) > 0
        threading.Thread.__init__(self)
        self.workers = map(lambda p: ReceiverWorker(self, p), ports)
        self.killed = threading.Event()
        self.amt = 0.0
        self.amt_lock = threading.Lock()
        self.trials = []
        self.display_results = display_results

    def start(self):
        map(lambda w: w.start(), self.workers)
        threading.Thread.start(self)

    def run(self):
        cur_trial = []
        start = time.time()
        while not self.killed.is_set():
            self.killed.wait(2.0)
            
            with self.amt_lock:
                amt = self.amt
                end = time.time()
                self.amt = 0.0
            if amt > 0:
                cur_trial.append((start, end, amt))
            else:
                if len(cur_trial) > 0:
                    self.trials.append(cur_trial)
                    if self.display_results:
                        bps = process_trial(cur_trial)
                        print ("%d\t%.2f\t%.3f" % 
                               (len(self.trials), bps, bps / (1000 * 1000)))
                    cur_trial = []
            start = end
    
    def stop(self):
        for w in self.workers:
            w.stop()
            w.join()
        self.killed.set()

    def update(self, amt):
        with self.amt_lock:
            self.amt += amt

    def get_trials(self):
        t = self.trials
        self.trials = []
        return t

def print_usage(exec_name=sys.argv[0]):
    print "Usage:"
    print "\t%s <port_start> <num_ports>" % exec_name
    sys.exit(1)

def main(argv):
    if len(argv) < 3 or not argv[1].isdigit() or not argv[2].isdigit():
        print_usage()
    ports = range(int(argv[1]), int(argv[1]) + int(argv[2]))

    r = Receiver(ports, display_results=True)
    r.start()
    try:
        while True:
            time.sleep(100)
    except KeyboardInterrupt:
        r.stop()
        r.join()

if __name__ == '__main__':
    main(sys.argv)
