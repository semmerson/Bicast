@mainpage Hycast Package

@tableofcontents

<hr>

@section overview Overview

The Bicast package is a distribution system for data-products that is based on the publish/subscribe
model and implemented using a one-to-many reliable multicast that combines source-specific multicast
(SSM) with peer-to-peer (P2P) networking. A data-product is an arbitrary sequence of 0 to (2^32)-1
(4294967295) bytes. The publish/subscribe model means that a subscriber make a subscription request
to a publisher and immdeately starts receiving (and processing) the subscribed-to data-products. The
reliable multicast attribute means that the publisher's bandwidth requirement is capped at a low
level. The SSM component is used on networks that support it to maximize throughput and minimize
latency. The P2P component is used to convey data segments to subscribers that were not delivered
via SSM and to completely handle data delivery on networks that don't support SSM.

<hr>

@section platform Platform Requirements

- An accurate and monotonic system clock (e.g., one that is synchronized by ntpd(8) or chronyd(8)).
- An operating system compliant with the POSIX.1-2017 standard. Most Linux and MacOS systems are
  fine.

<hr>

@section sysconfig Operating-System Configuration

The following items are necessary in order for a program to send and receive multicast UDP packets:
- A mapping must exist between multicast IP addresses and a network interface:
  - RHEL 6 & 7: The command `ip route` will show the mappings. A default mapping can be added via
    the command

        route add -net 240.0.0.0 netmask 240.0.0.0 dev <iface>
    where &lt;iface&gt; will be the default multicast interface (e.g., `eth0`).

- Any firewall on the host mustn't interfere:
  - iptables(8): This firewall can be completely disabled via the command `iptables -F`.

<hr>

@section installation Installation

- From the RPM:

<hr>

@section subscribing Subscribing to Data-Products

The `subscribe` program is used to subscribe to data-products. If all defaults are acceptable, then
it can be simply invoked as

    subscribe <pubAddr>
where &lt;pubAddr&gt; is the publisher's fully-qualified hostname or IP address. There are many
options, however, all of which can be viewed via the "-h" option:

    subscribe -h
As a conenience, all these runtime arguments can be specified in a YAML configuration-file:

    subscribe -c <config>
where &lt;config&gt; is the pathname of the configuration-file. The installed file
`etc/subscribe_example.yaml` is an example.

The disposition of received data-products is specified in another YAML file. The installed file
`etc/dispose_example.yaml` is an example.

<hr>

@section publishing Publishing Data-Products

This is the section on publishing data-products.