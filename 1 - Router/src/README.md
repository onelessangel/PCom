Copyright Teodora Stroe 321CA 2022

# The dataplane of a router

## ARP Protocol

If the message is an **ARP Request**, the Ethernet header is created and an ARP message is sent.

If the message is an **ARP Reply**:
- A new entry is created in the ARP cache;
- Every message in the queue of unsent messages is inspected and, if they are not of interest, they are saved in a temporary queue.
- For each message that needs to be forwarded, the ttl and checksum are recalculated, the Ethernet header is updated and the package is sent to the correct route.

## Forwarding

For each IPv4 packet the router performs the following steps:

- Checks if the package is corrupt by computing the checksum;
- Checks if the TTL of the message has not expired;
- Finds correct route to next hop by performing a *binary search*;
- Retrieves the MAC of the destination address from the ARP cache. If there is no valid entry, it sends and ARP request message and saves the current package in a local queue;
- Updates TTL and checksum of package by using the RFC1624 standard;
- Updates the Ethernet header and sends the package to the next hop.

## Efficient Longest Prefix Match (LPM) algorithm

In order to make the LPM algorithm more efficint the router uses a *binary search*.

## ICMP Protocol

There are three types of ICMP messages sent by the router:

- **Echo reply** ― the package is sent to the router itself;
- **Time exceeded** ― sent if the TTL has expired;
- **Destination unreachable** ― sent if there is no valid entry in the routing table.

## Checksum RFC 1624

Computes a fast checksum by using the RFC 1624 method:

```
HC' = ~(~HC + ~m + m')

HC  - old checksum in header
HC' - new checksum in header
m   - old value of TTL
m'  - new value of TTL
```

## References

1. [Routing laboratory](https://ocw.cs.pub.ro/courses/pc/laboratoare/04);
2. [Divide et Impera laboratory](https://ocw.cs.pub.ro/courses/pa/laboratoare/laborator-01);
3. [Computation of the internet checksum via incremental update](https://datatracker.ietf.org/doc/html/rfc1624);
3. Used functions from original skel: **build_ethhdr**, **send_icmp**, **send_icmp_error**, **send_arp**.