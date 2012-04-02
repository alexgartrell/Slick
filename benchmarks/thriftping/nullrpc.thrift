service NullRPCServer {
    oneway void ping(1: string id, 2: i64 tsc),
    void init(1: string ip, 2: i32 port),
}

service NullRPCClient {
    oneway void pong(1: i64 tsc),
}
