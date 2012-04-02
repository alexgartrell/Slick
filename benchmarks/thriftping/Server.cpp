/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "NullRPCServer.h"
#include "NullRPCClient.h"
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <server/TThreadedServer.h>
#include <server/TNonblockingServer.h>
#include <transport/TSocket.h>
#include <transport/TServerSocket.h>
#include <transport/TBufferTransports.h>
#include <tr1/unordered_map>
#include <map>
#include <pthread.h>
#include <iostream>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace std;
using boost::shared_ptr;


void *localStatsThreadLoop(void *p);
bool noDelay;

class NullRPCServerHandler : virtual public NullRPCServerIf {
private:
    tr1::unordered_map <string, NullRPCClientClient*> id_client_map;
    uint64_t num_requests;
public:
    NullRPCServerHandler() {
        num_requests = 0;
        pthread_t localStatsThreadId_;
        int code = pthread_create(&localStatsThreadId_, NULL,
                                  localStatsThreadLoop, this);
    }

    void dumpStats()
    {
        cout << "Requests per second: " << num_requests << endl;
        num_requests = 0;
    }

    void ping(const std::string& id, const int64_t tsc) {
        num_requests++;
        id_client_map[id]->pong(tsc);
    }

    void init(const std::string& ip, const int32_t port) {
        TSocket* sock = new TSocket(ip.data(), port);
        shared_ptr<TTransport> socket(sock);
        shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        NullRPCClientClient *c = new NullRPCClientClient(protocol);

        char sport[7];
        sprintf(sport, "%d", port);
        string sip = ip;
        string id = sip.append(":").append(sport, strlen(sport));
        id_client_map[id] = c;

        while(1) {
            try {
                sock->setNoDelay(noDelay);
                transport->open();
                break;
            } catch (TException &tx) {
                fprintf(stderr, "Transport error: %s\n", tx.what());
            }
            sleep(1);
        }
    }
};


void *localServerThreadLoop(void *p)
{
    int port = 9090;
    shared_ptr<NullRPCServerHandler> handler(new NullRPCServerHandler());
    shared_ptr<TProcessor> processor(new NullRPCServerProcessor(handler));
    shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    //TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
    //TNonblockingServer server(processor, port);


    server.serve();
    return NULL;
}

void *localStatsThreadLoop(void *p)
{
    NullRPCServerHandler *c = (NullRPCServerHandler*)p;

    while (1) {
        sleep(1);
        c->dumpStats();
    }
    return NULL;
}

int main(int argc, char **argv) {

    int ch;
    noDelay = false;
    while ((ch = getopt(argc, argv, "d")) != -1) {
        switch (ch) {
        case 'd':
            noDelay = true;
            break;
        default:
            exit(-1);
        }
    }
    pthread_t localServerThreadId_;
    int code = pthread_create(&localServerThreadId_, NULL,
                              localServerThreadLoop, NULL);

    pthread_join(localServerThreadId_, NULL);

    return 0;
}
