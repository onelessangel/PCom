#include <iostream>
#include <iomanip>
#include <cmath>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include "utils.hpp"
#include "utils_server.hpp"

using namespace std;

#define ARG_NO          2
#define SUBSCRIBED      "Subscribed to topic."
#define UNSUBSCRIBED    "Unsubscribed from topic."
#define INVALID_CMD     "invalid command\n"
#define INVALID_SF      "SF has invalid value\n"

class Server {
  public:
    void exec(char *port) {
        // create UDP socket file descriptor
        sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
        DIE(sockfd_udp < 0, "UDP socket creation failed");

        // set server address
        memset((char *) &servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(atoi(port));
        servaddr.sin_addr.s_addr = INADDR_ANY;

        // bind socket to server address
        ret = bind(sockfd_udp, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        DIE(ret < 0, "UDP bind failed");

        // initialize descriptor sets
        FD_ZERO(&read_fds);
        FD_ZERO(&tmp_fds);

        // create TCP socket file descriptor
        sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
        DIE(sockfd_tcp < 0, "TCP socket creation failed");

        // deactivate Nagle algorithm
        int nagle_flag = 1;
        ret = setsockopt(sockfd_tcp, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle_flag, sizeof(int));
        DIE(ret == -1, "Nagle deactivation failed");

        // bind TCP socket to server address
        ret = bind(sockfd_tcp, (const struct sockaddr *) &servaddr, sizeof(servaddr));
        DIE(ret < 0, "TCP bind failed");

        ret = listen(sockfd_tcp, MAX_BACKLOG);
        DIE(ret < 0, "listen failed");

        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd_udp, &read_fds);
        FD_SET(sockfd_tcp, &read_fds);
        fdmax = max(sockfd_udp, sockfd_tcp);

        while (1) {
            tmp_fds = read_fds; 
		
            ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
            DIE(ret < 0, "select failed");

            if (FD_ISSET(STDIN_FILENO, &tmp_fds)) { // read stdin input
                string command;
                getline(cin, command);

                if (command == EXIT) {
                    while (!connected_clients.empty()) {
                        auto cl = connected_clients.begin();
                        send_message(EXIT, cl->sockfd);
                        close(cl->sockfd);
                        connected_clients.erase(cl);
                    }                 
                    break;
                } else {
                    cerr << "(" << __FILE__ << ", " << __LINE__ << "): ";
                    cerr << INVALID_CMD;
                }
            }

            for (int i = 1; i <= fdmax; i++) {
                if (!FD_ISSET(i, &tmp_fds)) {
                    continue;
                }

                if (i == sockfd_udp) {
                    udp_msg = read_udp_message(sockfd_udp);
                    send_to_subscribers(udp_msg);
                } else if (i == sockfd_tcp) {   // new connection request on TCP socket
                    clilen = sizeof(cliaddr);

                    // get new client socket
                    sockfd_newcli = accept(sockfd_tcp, (struct sockaddr *) &cliaddr, &clilen);
                    DIE(sockfd_newcli < 0, "accept failed");

                    // get new client ID and store it in buffer

                    // get message length
                    uint32_t msg_len;
                    n = recv(sockfd_newcli, &msg_len, sizeof(uint32_t), 0);
                    DIE(n < 0, "recv failed");

                    // get actual message
                    memset(buffer, 0, BUF_LEN);
                    n = recv(sockfd_newcli, buffer, msg_len, 0);
                    DIE(n < 0, "recv failed");

                    if (client_is_connected(buffer)) {
                        cout << "Client " << buffer << " already connected.\n";
                        send_message(EXIT, sockfd_newcli);
                        close(sockfd_newcli);
                        continue;
                    }
                    
                    connected_clients.insert({sockfd_newcli, buffer});
                    
                    // add new TCP socket to read_fds and update fdmax
                    FD_SET(sockfd_newcli, &read_fds);
                    fdmax = max(fdmax, sockfd_newcli);

                    cout << "New client " << buffer << " connected from " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << ".\n";
                   
                    check_store_and_forward(sockfd_newcli, buffer);
                } else {    // new message from existing TCP connection
                    uint32_t msg_len;
                    n = recv(i, &msg_len, sizeof(uint32_t), 0);
                    DIE(n < 0, "recv failed");

                    auto it_client = find_if(connected_clients.begin(), connected_clients.end(),
                                                [&](const struct client &cl){return cl.sockfd == i;});

                    if (n == 0) {   // closed connection
                        auto id = it_client->ID;

                        disconnect_client(it_client);
                        close(i);
                        FD_CLR(i, &read_fds);

                        cout << "Client " << id << " disconnected.\n";
                    } else {    // receives message
                        memset(buffer, 0, BUF_LEN);
                        n = recv(i, buffer, msg_len, 0);
                        DIE(n < 0, "recv failed");

                        string cmd, topic, SF;
                        if (!get_command(cmd, topic, SF)) {
                            continue;
                        }

                        if (cmd == "subscribe") {
                            if (client_is_subscribed(topic, it_client->ID)) {
                                update_subscriber(topic, it_client->ID, SF == "1");
                                continue;
                            }
                            
                            add_subscriber(topic, {i, it_client->ID, SF == "1", true});

                            send_message(SUBSCRIBED, i);
                        } else {                           
                            if (!client_is_subscribed(topic, it_client->ID)) {
                                continue;
                            }

                            remove_subscriber(topic, it_client->ID);

                            send_message(UNSUBSCRIBED, i);
                        }
                    }
                }
            }
        }

        close(sockfd_tcp);
        close(sockfd_udp);
    }

  private:
    int sockfd_udp;              // UDP socket file descriptor
    int sockfd_tcp;              // TCP socket file descriptor
    int sockfd_newcli;           // new client socket file descriptor
    int ret, n;
    struct sockaddr_in servaddr; // server address
    struct sockaddr_in cliaddr;  // server address
    socklen_t clilen;
    fd_set read_fds, tmp_fds;
    int fdmax;			         // valoare maxima fd din multimea read_fds
    udp_message udp_msg;
    char buffer[BUF_LEN];
    unordered_map<string, vector<subscriber>> subscribers_DB;   // subscribers data base
    set<client> connected_clients;                              // set containing connected clients
    
    udp_message read_udp_message(int sockfd_udp) {
        udp_message msg;
        ostringstream number_stream;
        struct sockaddr_in cliaddr, subaddr;
        socklen_t len = sizeof(cliaddr);

        // clear buffer
        memset(buffer, 0, BUF_LEN);

        // clear client address
        memset(&cliaddr, 0, len);

        // receive udp_message from UDP client and store it in buffer
        n = recvfrom(sockfd_udp, buffer, BUF_LEN, 0, (struct sockaddr *)&cliaddr, &len);
        DIE(n < 0, "recvfrom failed");

        // set client IP and port
        msg.udp_client_ip = inet_ntoa(cliaddr.sin_addr);
        msg.udp_client_port = ntohs(cliaddr.sin_port);

        // set payload
        switch (Type(buffer[TOPIC_LEN])) {
        case INT:
            uint32_t number_int;
            uint8_t sign_int;

            // get sign and number
            memcpy(&sign_int, &buffer[TOPIC_LEN + 1], sizeof(uint8_t));
            memcpy(&number_int, &buffer[TOPIC_LEN + 2], sizeof(uint32_t));

            // set payload
            if (sign_int == 1) {
                msg.payload.push_back('-');
            }
            msg.payload.append(to_string(ntohl(number_int)));

            // set type identifier
            msg.data_type = TYPE_INT;
            break;
        
        case SHORT_REAL:
            uint16_t number_short;
            float correct_short;
            
            memcpy(&number_short, &buffer[TOPIC_LEN + 1], sizeof(uint16_t));

            correct_short = float(ntohs(number_short)) / 100;

            if (correct_short == int(correct_short)) {
                msg.payload = to_string(int(correct_short));
            } else {
                number_stream.precision(2);
                number_stream << fixed << correct_short;
                msg.payload = number_stream.str();
            }

            // set type identifier
            msg.data_type = TYPE_SHORT_REAL;
            break;
        
        case FLOAT:
            uint32_t number_float;
            uint8_t power;
            uint8_t sign_float;
            float correct_float;

            // get sign, number and power
            memcpy(&sign_float,  &buffer[TOPIC_LEN + 1], sizeof(uint8_t));
            memcpy(&number_float, &buffer[TOPIC_LEN + 2], sizeof(uint32_t));
            memcpy(&power, &buffer[TOPIC_LEN + 2 + sizeof(uint32_t)], sizeof(uint8_t));

            correct_float = float(ntohl(number_float)) / pow(10, int(power));

            // set payload
            if (sign_float == 1) {
                msg.payload.push_back('-');
            }

            number_stream.precision(int(power));
            number_stream << fixed << correct_float;
            msg.payload.append(number_stream.str());
            
            // set type identifier
            msg.data_type = TYPE_FLOAT;
            break;

        case STRING:
            msg.payload.append(buffer + TOPIC_LEN + 1);
            
            // set type identifier
            msg.data_type = TYPE_STRING;
            break;

        default:
            break;
        }

        // set topic
        buffer[TOPIC_LEN] = '\0';
        msg.topic = strtok(buffer, "\0");

        return msg;
    }

    void send_to_subscribers(udp_message msg) {
        if (!subscribers_DB.count(msg.topic)) {
            return;
        }

        string message = create_tcp_message(msg);

        for (auto &subscriber : subscribers_DB[msg.topic]) {
            if (subscriber.connected) {
                send_message(message, subscriber.sockfd);
            } else if (subscriber.SF) {
                subscriber.unsent_messages.push(message);
            }
        }
    }

    string create_tcp_message(udp_message msg) {
        stringstream message;
        message << msg.udp_client_ip << ":" << msg.udp_client_port << " - " << msg.topic << " - " << msg.data_type << " - " << msg.payload;
        return message.str();
    }

    void send_message(string message, int sockfd) {
        uint32_t msg_len = message.length();
        n = send(sockfd, &msg_len, sizeof(uint32_t), 0);
        DIE(n < 0, "send failed");

        n = send(sockfd, message.c_str(), message.length(), 0);
        DIE(n < 0, "send failed");
    }

    void check_store_and_forward(int sockfd, char *id) {
        for (auto &entry : subscribers_DB) {
            auto it_sub = find_if(subscribers_DB[entry.first].begin(), subscribers_DB[entry.first].end(),
                                    [&](const struct subscriber &sub){return sub.ID == id;});
            
            // topic has subscriber with given ID
            if (it_sub != subscribers_DB[entry.first].end()) {
                it_sub->sockfd = sockfd;
                it_sub->connected = true;
                
                if (it_sub->SF) {
                    while (!it_sub->unsent_messages.empty()) {
                        send_message(it_sub->unsent_messages.front(), sockfd);
                        it_sub->unsent_messages.pop();
                    }
                }
            }
        }
    }

    bool client_is_connected(char *id) {
        return find_if(connected_clients.begin(), connected_clients.end(),
                        [&](const struct client &cl){return cl.ID == id;}) != connected_clients.end();
    }

    void disconnect_client(set<client>::iterator cl) {
        // set subscriber as 'not connected'
        for (auto &entry : subscribers_DB) {
            auto &subscribers = entry.second;
            auto subscriber = find_if(subscribers.begin(), subscribers.end(),
                                        [&](const struct subscriber &sub){return sub.ID == cl->ID;});

            subscriber->connected = false;
        }

        // delete client from connected_clients
        connected_clients.erase(cl);
    }

    bool get_command(string &cmd, string &arg1, string &arg2) {
        cmd = strtok(buffer, " \n");

        if (cmd != "subscribe" && cmd != "unsubscribe") {
            cerr << "(" << __FILE__ << ", " << __LINE__ << "): ";
            cerr << INVALID_CMD;
            return false;
        }

        arg1 = strtok(NULL, " \n");

        if (cmd == "subscribe") {
            arg2 = strtok(NULL, " \n");

            if (arg2 != "0" && arg2 != "1") {
                cerr << "(" << __FILE__ << ", " << __LINE__ << "): ";
                cerr << INVALID_SF;
                return false;
            }
        }

        return true;
    }

    bool client_is_subscribed(string topic, string id) {
        if (!subscribers_DB.count(topic)) {
            return false;
        }

        auto topic_subscribers = subscribers_DB[topic];
        return find_if(topic_subscribers.begin(), topic_subscribers.end(),
                    [&](const struct subscriber &sub){return sub.ID == id;}) != topic_subscribers.end();
        
    }

    void update_subscriber(string topic, string id, bool SF) {
        auto it_sub = find_if(subscribers_DB[topic].begin(), subscribers_DB[topic].end(),
                                [&](const struct subscriber &sub){return sub.ID == id;});
        it_sub->SF = SF;
    }

    void add_subscriber(string topic, subscriber sub) {
        subscribers_DB[topic].push_back(sub);
    }

    void remove_subscriber(string topic, string id) {
        auto it_sub = find_if(subscribers_DB[topic].begin(), subscribers_DB[topic].end(),
                                [&](const struct subscriber &sub){return sub.ID == id;});

        subscribers_DB[topic].erase(it_sub);
    }

    // DEBUG function
    void print_udp_msg(udp_message udp_msg) {
        cout << "Message IP address: " << udp_msg.udp_client_ip << '\n';
        cout << "Message port: " << udp_msg.udp_client_port << '\n';
        cout << "Message topic: " << udp_msg.topic << '\n';
        cout << "Message data_type: " << udp_msg.data_type << '\n';
        cout << "Message payload: " << udp_msg.payload << '\n';
        cout << '\n';
    }
};

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != ARG_NO, "incorrect usage");

    auto *server = new (nothrow) Server();
    DIE(!server, "server failed");
    server->exec(argv[1]);
    delete server;

    return 0;
}