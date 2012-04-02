service ForwarderController {
  oneway void add_server(1: string ip, 2: i32 port, 3: i32 key),
  oneway void release_servers()
}

service Forwarder {
  oneway void msg(1: i32 key, 2: string payload)
}