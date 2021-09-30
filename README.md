# d2prices
## D2 Price Collector

1. Decompile the Invoker using JPEXS and extract the target protocol ID (as IDs are shuffled with each update).

2. Compile the application with: ```gcc live_decoder.c -lsqlite3 -o live```

3. Using tshark sniff the traffic from the server and output packet payloads to stdout, then pipe the output to the executable and pass the target protocol ID as the first argument: ```tshark -l -i any -Tfields -e data tcp src port 5555 2>/dev/null | ./live <target_proto_id>```

### Dependencies
1. libsqlite3-dev
2. tshark