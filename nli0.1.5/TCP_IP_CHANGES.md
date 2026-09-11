# TCP IP changes

- Removed the packed 32-bit numeric IP format.
- Nextion now stores the destination as readable IPv4 text in `setting.tcp_ip.txt`.
- C++ queries the Text component with `get setting.tcp_ip.txt` and expects the Nextion `0x70` String Data response.
- C++ validates and caches the dotted IPv4 string.
- Python keeps `ESP_IP = None` and obtains the cached address through the `get_tcp_ip` Bridge RPC.
- `tcp_client.py` refreshes its host from C++ before connection/reconnection.
- See `NEXTION_TCP_IP_SETUP.txt` for the HMI configuration.
