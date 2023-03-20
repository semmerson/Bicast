@mainpage Hycast Package

@section contents Table of Contents
- \ref sysconfig

<hr>

@section sysconfig Operating-System Configuration

The following items are necessary in order for a program to send and receive multicast UDP packets:
- A mapping must exist between multicast IP addresses and a network interface:
  - RHEL 6 & 7: The command `ip route` will show the mappings. A default mapping can be added via
    the command

        route add -net 240.0.0.0 netmask 240.0.0.0 dev <iface>

    where <iface> will be the default multicast interface (e.g., `eth0`).

- Any firewall on the host mustn't interfere:
  - iptables(8): This firewall can be completely disabled via the command `iptables -F`.
