/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "timing.h"
#include "NullRPCClient.h"
#include "NullRPCServer.h"
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <transport/TSocket.h>
#include <transport/TServerSocket.h>
#include <transport/TBufferTransports.h>
#include <list>

#include <iostream>
#include <string>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;
using namespace std;

uint64_t received;
list<int32_t> rt_list;
int threshold = 200;

class NullRPCClientHandler : virtual public NullRPCClientIf {
public:
    NullRPCServerClient* server;
    shared_ptr<TTransport> my_transport;
    int myPort;
    string myIP;

    NullRPCClientHandler(string localIP, int listenport) {
        myPort = listenport;
        myIP = localIP;
    }

    void init(string serverIP, string myIP, int remotePort) {
        shared_ptr<TTransport> socket(new TSocket(serverIP.data(), remotePort));
        shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        my_transport = transport;
        shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        server = new NullRPCServerClient(protocol);
        received = 0;
        while (1) {
            try {
                transport->open();
                break;
            } catch (TException &tx) {
                fprintf(stderr, "Transport error: %s\n", tx.what());
            }
            sleep(3);
        }

        server->init(myIP, myPort);
    }

    void pong(const int64_t tsc) {
        unsigned long long etsc = rdtsc();
        //printf("Response Ticks: %lld\n", (etsc - tsc));
        rt_list.push_back(etsc - tsc);
        received++;
    }

    void bench(uint64_t num_pings)
    {
        char sport[7];
        sprintf(sport, "%d", myPort);
        string id = myIP.append(":").append(sport, strlen(sport));
        uint64_t sent = 0;
        struct timespec req;
        req.tv_sec = 0;
        req.tv_nsec = 50000;

        while (sent < num_pings) {
            unsigned long long tsc = rdtsc();
            server->ping(id, tsc);
            sent++;
            if (sent - received > threshold)
                nanosleep(&req, NULL);
        }
    }
};

void *localServerThreadLoop(void *p)
{
    NullRPCClientHandler* n = (NullRPCClientHandler*)p;
    shared_ptr<NullRPCClientHandler> handler(n);
    shared_ptr<TProcessor> processor(new NullRPCClientProcessor(handler));
    shared_ptr<TServerTransport> serverTransport(new TServerSocket(n->myPort));
    shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    server.serve();
    return NULL;
}

void print_rt_dist(int cpu_MHZ) {
    cout << "Response Time (us) Distribution: (CPU Speed: " << cpu_MHZ << " MHz)" << endl;
    list<int32_t>::iterator it;
    for (it=rt_list.begin(); it!=rt_list.end(); it++)
        cout << (*it) / cpu_MHZ << endl;
}

void usage()
{
    cerr <<   "./nullrpcc [-n numpings] [-i selfIP] [-p selfPort] [-c CPUSpeed] [-t threshold] ServerIP \n"
         <<   "   -n #       num pings to send \n"
         <<   "   -i IP      self IP addr \n"
         <<   "   -p port    self port \n"
         <<   "   -c MHz     CPU speed in MHz (to calculate response time) \n"
         <<   "   -t #       Number of outstanding RPCs to keep in flight \n"
         <<   "   -r port    remotePort\n";

    cerr <<   "Example: ./nullrpcc -i fawn-desktop2 -p 8000 -n 10000 ServerIP" << endl;

}

int main(int argc, char **argv)
{
    int ch;
    string serverIP, myIP;
    int myPort = 11000;
    int remotePort = 9090;
    uint64_t num_pings = 1;
    int cpu_MHZ = 2775;
    while ((ch = getopt(argc, argv, "i:p:n:r:c:t:")) != -1) {
        switch (ch) {
        case 'i':
            myIP = optarg;
            break;
        case 't':
            threshold = atoi(optarg);
            break;
        case 'c':
            cpu_MHZ = atoi(optarg);
            break;
        case 'p':
            myPort = atoi(optarg);
            break;
        case 'r':
            remotePort = atoi(optarg);
            break;
        case 'n':
            num_pings = atoi(optarg);
            break;
        default:
            exit(-1);
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 1) {
        usage();
        exit(-1);
    }

    serverIP = string(argv[0]);

    NullRPCClientHandler* n = new NullRPCClientHandler(myIP, myPort);

    pthread_t localServerThreadId_;
    int code = pthread_create(&localServerThreadId_, NULL,
                              localServerThreadLoop, n);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    n->init(serverIP, myIP, remotePort);
    n->bench(num_pings);
    gettimeofday(&end, NULL);
    double t = timeval_diff(&start, &end);
    printf("Time: %f, Rate:%f\n", t, (double)(num_pings/t));

    while (received != num_pings) {
        sleep(1);
    }

    print_rt_dist(cpu_MHZ);
    //pthread_join(localServerThreadId_, NULL);

    return 0;
}
