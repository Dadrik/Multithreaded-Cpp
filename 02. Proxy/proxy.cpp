#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>

using namespace std;

#define MAX_CONN 4
#define EVENT_NUM 10
#define BUF_SIZE 1024

int main() {
    // Listeneres
    int sockets[MAX_CONN];
    for (int i = 0; i < MAX_CONN; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(1234 + i);
        addr.sin_addr.s_addr = INADDR_ANY;
        bind(sockets[i], (struct sockaddr *) &addr, sizeof (addr));
        listen(sockets[i], 10);
        cout << "[LISTEN] socket " << sockets[i] << endl;
    }

    // Receiver
    int out_sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(out_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        cerr << "destination server unavailible" << endl;
        return 1;
    }
    cout<< "[CONN] socket " << out_sock << endl;

    // EPOLL init
    int epfd = epoll_create(1);
    if (epfd < 0) {
        cerr << "epoll_create failed" << endl;
        return 1;
    }
    epoll_event ev, events[EVENT_NUM];
    for (int i = 0; i < MAX_CONN; i++) {
        ev.data.fd = sockets[i];
        ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockets[i], &ev) == -1) {
            cerr << "epoll_ctl failed" << endl;
            return 1;
        }
    }

    // Main cycle
    while (true) {
        int nfds = epoll_wait(epfd, events, EVENT_NUM, -1);
        if (nfds < 0) {
            cerr << "epoll_wait failed" << endl;
            return 1;
        }
        // Event handling
        for (int n = 0; n < nfds; n++) {
            int check_id;

            // Listeners work
            for (check_id = 0; check_id < MAX_CONN; check_id++) {
                if (sockets[check_id] == events[n].data.fd) {
                    sockaddr_in client;
                    socklen_t socklen;
                    int conn_sock = accept(sockets[check_id], (struct sockaddr *) &client, &socklen);
                    if (conn_sock < 0) {
                        cerr << "accept failed" << endl;
                        return 1;
                    }
                    cout << "[ACC] socket = " << conn_sock << endl;
                    ev.events = EPOLLIN | EPOLLHUP;
                    ev.data.fd = conn_sock;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                        cerr << "epoll_ctl failed" << endl;
                        return 1;
                    }
                    break;
                }
            }

            // Proxy work
            if (check_id >= MAX_CONN) {
                if (events[n].events & EPOLLIN) {
                    char buf[BUF_SIZE];
                    int data_in = recv(events[n].data.fd, buf, BUF_SIZE, 0);
                    if (data_in < 0) {
                        cerr << "recv failed" << endl;
                        return 1;
                    } else if (data_in == 0) {
                        cout << "[CLOSE] socket " << events[n].data.fd << endl;
                        close(events[n].data.fd);
                        continue;
                    }

                    cout << "[RECV] " << data_in << " bytes from " << events[n].data.fd << endl;

                    int data_out = send(out_sock, buf, data_in, 0);
                    if (data_out < 0) {
                        cerr << "send failed" << endl;
                        return 1;
                    }
                    cout << "[SEND] " << data_out << " bytes to " << out_sock << endl;
                } else if (events[n].events & EPOLLHUP) {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[n].data.fd, &events[n]) != 0) {
                        cerr << "epoll_ctl failed" << endl;
                        return 1;
                    }
                    cout << "[CLOSE] socket " << events[n].data.fd << endl;
                    close(events[n].data.fd);
                }
            }
        }
    }
    for (int i = 0; i < MAX_CONN; i++)
        close(sockets[i]);
    close(out_sock);
    return 0;
}