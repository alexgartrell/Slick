from forwarder import Forwarder

import socket
from thrift import Thrift
from thrift.transport import TTransport
from thrift.transport import TSocket
from thrift.protocol import TBinaryProtocol


import threading

class SenderWorker(threading.Thread):
    def __init__(self, addr, payload_size, iters):
        threading.Thread.__init__(self)
        self.addr = addr
        self.payload_size = payload_size
        self.iters = iters

    def run(self):
        f = open('payloads/payload%d.tft' % self.payload_size)
        b = f.read()
        f.close()

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(self.addr)
        for i in xrange(self.iters):
            sock.sendall(b)
        sock.close()

class Sender(threading.Thread):
    def __init__(self, addr, keys, payload_size, iters, nthreads):
        threading.Thread.__init__(self)
        self.workers = []
        nthreads=1
        for i in xrange(nthreads):
            self.workers.append(SenderWorker(addr, payload_size, iters))

    def run(self):
        for w in self.workers:
            w.start()
        for w in self.workers:
            w.join()

if __name__ == '__main__':
    import sys
    sock = TSocket.TSocket('127.0.0.1', 12345)
    trans = TTransport.TBufferedTransport(sock)
    proto = TBinaryProtocol.TBinaryProtocol(trans)
    client = Forwarder.Client(proto)

    trans.open()

    payload_size = int(sys.argv[1])

    for k in xrange(20):
        client.send_msg(k, 'x'*payload_size)

    trans.close()
