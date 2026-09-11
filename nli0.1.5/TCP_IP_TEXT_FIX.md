# Nextion TCP IP Text fix

The `tcp_ip` HMI component is a Text component, not a numeric variable.

Final path:

`setting.tcp_ip.txt` -> UART -> Arduino C++ cache -> `get_tcp_ip` Bridge RPC -> Python TCP client.

Example value entered in Nextion:

`172.30.1.6`

No numeric packing is used.
