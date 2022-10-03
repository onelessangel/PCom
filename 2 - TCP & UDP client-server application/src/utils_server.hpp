#ifndef _UTILS_SERVER_HPP
#define _UTILS_SERVER_HPP

#include <iostream>
using namespace std;

#define TOPIC_LEN 50
#define MAX_BACKLOG 128
#define TYPE_INT "INT"
#define TYPE_SHORT_REAL "SHORT_REAL"
#define TYPE_FLOAT "FLOAT"
#define TYPE_STRING "STRING"

enum Type {INT, SHORT_REAL, FLOAT, STRING};

struct udp_message {
    string udp_client_ip;
    int udp_client_port;
    string topic;
    string data_type;
    string payload;
};

struct subscriber {
    int sockfd;
    string ID;
    bool SF;
    bool connected;
    queue<string> unsent_messages;
};

struct client {
    int sockfd;
    string ID;

    bool operator <(const client& cl) const {
        return ID < cl.ID;
    }
};

#endif
