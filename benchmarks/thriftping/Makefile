all: nullrpcc nullrpcs

GEN_SRC = gen-cpp/NullRPCClient.cpp gen-cpp/NullRPCServer.cpp timing.c
BOOST_DIR = /usr/include/boost/
CFLAGS = $(shell pkg-config --cflags thrift-nb) -I${BOOST_DIR} -Igen-cpp
LIBS = $(shell pkg-config --libs thrift-nb) -levent

nullrpcc: Client.cpp nullrpc.thrift timing.c
	thrift --gen cpp nullrpc.thrift
	g++ -o $@ ${CFLAGS} ${LIBS} $< ${GEN_SRC}

nullrpcs: Server.cpp nullrpc.thrift timing.c
	thrift --gen cpp nullrpc.thrift
	g++ -o $@ ${CFLAGS} ${LIBS} $< ${GEN_SRC}

clean:
	rm -f *~ nullrpcc nullrpcs *.o *.lo
