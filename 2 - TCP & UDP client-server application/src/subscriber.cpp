#include <iostream>
#include <iomanip>
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
#include "utils.hpp"

using namespace std;

#define ARG_NO 4

class Subscriber {
  public:
    void exec(char *ID, char *server_ip, char *server_port) {
        // initialize descriptor sets
        FD_ZERO(&read_fds);
        FD_ZERO(&tmp_fds);

        // create socket file descriptor
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        DIE(sockfd < 0, "socket creation failed");

        // deactivate Nagle algorithm
        int nagle_flag = 1;
        ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle_flag, sizeof(int));
        DIE(ret == -1, "Nagle deactivation failed");

        // set server address
        memset((char *) &servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(atoi(server_port));
        ret = inet_aton(server_ip, &servaddr.sin_addr);
        DIE(ret == 0, "inet_aton failed");

        // connect to server
        ret = connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
        DIE(ret < 0, "connection failed");

        // send ID
        send_message(ID, sockfd);

        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        while (1) {
            tmp_fds = read_fds; 

            ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
            DIE(ret < 0, "select failed");

            if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
                string command;
                getline(cin, command);

                if (command == EXIT) {
                    break;
                }

                send_message(command, sockfd);
            }

            if (FD_ISSET(sockfd, &tmp_fds)) {
                uint32_t msg_len;
                n = recv(sockfd, &msg_len, sizeof(uint32_t), 0);
                DIE(n < 0, "recv failed");

                memset(buffer, 0, BUF_LEN);
                n = recv(sockfd, buffer, msg_len, 0);
                DIE(n < 0, "recv failed");

                if (strncmp(buffer, EXIT, strlen(EXIT)) == 0) {
                    break;
                }
                
                cout << buffer << '\n';
            }
        }

        close(sockfd);
    }
  
  private:
    int sockfd, ret, n;
    struct sockaddr_in servaddr;
    char buffer[BUF_LEN];
    fd_set read_fds, tmp_fds;

    void send_message(string message, int sockfd) {
        uint32_t msg_len = message.length();
        n = send(sockfd, &msg_len, sizeof(uint32_t), 0);
        DIE(n < 0, "send failed");

        n = send(sockfd, message.c_str(), message.length(), 0);
        DIE(n < 0, "send failed");
    }
};

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != ARG_NO, "incorrect usage");

    auto *subscriber = new (nothrow) Subscriber();
    DIE(!subscriber, "subscriber failed");
    subscriber->exec(argv[1], argv[2], argv[3]);
    delete subscriber;

    return 0;
}