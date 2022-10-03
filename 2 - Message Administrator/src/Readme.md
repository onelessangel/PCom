Copyright Teodora Stroe 321CA 2022

# TCP & UDP client-server application for message handling

## 1. FILE GUIDE
-------------

```
.
├── Makefile            -- Makefile
├── readme.txt          -- README
├── server.cpp          -- server
├── subscriber.cpp      -- TCP client
├── utils_server.hpp    -- server structures, enums and macros
└── utils.hpp           -- common macros and DIE macro
```

-------------
## 2. STRUCTURES
-------------

### i. udp_message:
- __used for__:   storing UDP messages
- __containing__: UDP client IP, UDP client port, topic, data type, payload

### ii. subscriber
- __used for__:   memorizing information for each topic's subscriber
- __containing__: socket file descriptor, ID, SF, state (connected or not),
                  queue of unsent messages

### iii. client
- __used for__:   memorizing basic information about a TCP client
- __containing__: socket file descriptor, ID

---------------
## 3. PROGRAM FLOW
---------------

Both the server and the subscriber depend on the correct execution of the executable.

At the beginning of the programs, the display buffering is deactivated and the Nagle
algorithm is disabled.

## -- THE SERVER --

It can store unlimited clients/messages due to the usage of _std::vector_ and _std::queue_
structures.

The server retains:
- _<connected_clients>_ -- a set of connected TCP clients
- _<subscribers_DB>_    -- a subscribers database for each topic

When the server receives a new request on the UDP socket:
- the message is read and stored into a buffer
- a udp_message is created, storing the UDP client data, the topic, data type and the custom payload
- the TCP message is created based on the udp_message information
- the message is sent to each subscriber in the message's topic list if connected.
      or otherwise stored in the unsent message queue if SF is activated

When the server receives a new request on the TCP socket:
- if it's a new connection request:
    - receives the subscriber's ID;
    - if the client is connected shuts it down;
    - otherwise:
        - inserts it into the connected clients set
        - sends it the missed messages from the topics with SF activated
        - updates subscriber details in each topic's list

- if it's an already existing connection request:
    - if it's a closed connection, deletes client from connected_clients and updates subscribers connection status
    - otherwise, receives the command, updates subscribers and adds/removes subscribers as necessary

When the server receives _EXIT_ from user input:
- sends EXIT message to all connected clients
- closes clients' sockets

## -- THE SUBSCRIBER --

When first connected, sends ID to the server.

If it receives a meesage from the server:
- it it's EXIT, it closes;
- otherwise, displays the message.

If it receives the EXIT command from the user, it closes.

---------------
## 3. TCP PROTOCOL
---------------

- The operations are performed efficiently, each client only receiving messages for
one of his topics.
- The _Nagle algorithm_ is deactivated, so the commands take effect immediately.
- The protocol uses length prefixing for message framing, first sending the length
of the message as an unsigned 4-byte number (uint32_t), then the message itself.
This approach was chosen to prevent truncation / concatenation, which would have
been more likely to occur when sending / reading messages with a fixed length.

-----------------
## 4. ERROR HANDLING
-----------------

The errors reported by the "socket.h" API are handled using the DIE macro.

Any incorrect user input is ignored by the program and, for a non-existent command,
a message is displayed to STDERR in order not to load the STDOUT stream.

A possible command for maximizing the program's efficiency and ease debugging would be:
`./server 1234 2>server.log`

----------
## 5. TESTING
----------

Current solution has been tested manually and using the checker for efficiency
with more than 100 fast messages.

----------
## 6. REFERENCES
----------

[1] TCP/IP Protocol Design: Message Framing, https://www.codeproject.com/Articles/37496/TCP-IP-Protocol-Design-Message-Framing

[2] Message Framing in REST, https://theamiableapi.com/2012/04/01/message-framing-in-rest/

[3] Creating a Simple TCP Message Protocol, https://levelup.gitconnected.com/creating-a-simple-tcp-message-protocol-5bcc335ad1e7

[4] Protocol framing, https://snoozetime.github.io/2019/02/20/TCP-framing-protocol.html