from forwarder import ForwarderController

from thrift import Thrift
from thrift.transport import TTransport
from thrift.transport import TSocket
from thrift.protocol import TBinaryProtocol


class Controller:
    def __init__(self, addr):
        self.addr = addr
        self.sock = None
        self.trans = None
        self.proto = None
        self.client = None

    def open(self):
        self.sock = TSocket.TSocket(self.addr[0], self.addr[1])
        self.trans = TTransport.TBufferedTransport(self.sock)
        self.proto = TBinaryProtocol.TBinaryProtocol(self.trans)
        self.client = ForwarderController.Client(self.proto)
        self.trans.open()
        
    def add_server(self, ip, port, key):
        self.client.add_server(ip, port, key)

    def release_servers(self):
        self.client.release_servers()

    def close(self):
        self.trans.close()
        self.sock = None
        self.trans = None
        self.proto = None
        self.client = None
