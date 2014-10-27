#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <memory>
#include <string>

#include <cstring>

#include <sys/epoll.h>
#include <event.h>


using namespace std;

#define BUF_SIZE 1024

struct Client {
    string ip;
    queue<shared_ptr<string>> buf;
};

void server(unsigned port = 12345) {
    // Server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0) {
        cerr << "server socket error";
        exit(1);
    }
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
        cerr << "bind failed";
        exit(1);
    }
    if (listen(server_socket, 10) < 0) {
        cerr << "listen failed";
        exit(1);
    }

    // Clients and buffers
    unordered_map<int, Client> clients;

    // EPOLL init
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        cerr << "epoll_create failed" << endl;
        exit(1);
    }
    epoll_event ev, event;
    ev.data.fd = server_socket;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
        cerr << "epoll_ctl failed" << endl;
        exit(1);
    }

    // Main cycle
    while (true) {
        int nfds = epoll_wait(epfd, &event, 1, -1);
        if (nfds < 0) {
            cerr << "epoll_wait failed" << endl;
            exit(1);
        }
        // Event handling
        for (int n = 0; n < nfds; n++) {
            // Accept client
            if (event.data.fd == server_socket) {
                // Accept new client
                sockaddr_in client_addr;
                socklen_t socklen;
                int connect_sock = accept(server_socket, (struct sockaddr *) &client_addr, &socklen);
                if (connect_sock < 0) {
                    cerr << "accept failed" << endl;
                    exit(1);
                }
                cout << "[ACC] socket = " << connect_sock << endl;
                Client client;
                client.ip = inet_ntoa(client_addr.sin_addr);
                clients.emplace(connect_sock, client);
                // Add client event
                ev.events = EPOLLIN | EPOLLOUT | EPOLLHUP;
                ev.data.fd = connect_sock;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connect_sock, &ev) == -1) {
                    cerr << "epoll_ctl failed" << endl;
                    exit(1);
                }
            // Read data
            } else if (event.events & EPOLLIN) {
                int fd = event.data.fd;
                char buf[BUF_SIZE];
                ssize_t data_in = recv(fd, buf, BUF_SIZE, 0);
                if (data_in < 0) {
                    cerr << "recv failed" << endl;
                    exit(1);
                } else if (data_in == 0) {
                    cout << "[CLOSE] socket " << fd << endl;
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event) != 0) {
                        cerr << "epoll_ctl failed" << endl;
                        exit(1);
                    }
                    clients.erase(fd);
                    close(fd);
                    continue;
                }
                buf[data_in] = '\0';
                string str = string(buf);
                shared_ptr<string> ptr(new string("[" + clients[fd].ip + "] " + str));
                //cout << "[RECV] " << *ptr << endl;
                for (auto i = clients.begin(); i != clients.end(); i++) {
                    if (i->first != fd) {
                        i->second.buf.push(ptr);
                    }
                }
            // Close socket
            } else if (event.events & EPOLLHUP) {
                int fd = event.data.fd;
                cout << "[CLOSE] socket " << fd << endl;
                if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event) != 0) {
                    cerr << "epoll_ctl failed" << endl;
                    exit(1);
                }
                clients.erase(fd);
                close(fd);
            // Write data
            } else if (event.events & EPOLLOUT) {
                int fd = event.data.fd;
                if (clients[fd].buf.empty())
                    continue;

                //for ( ; !clients[fd].buf.empty(); clients[fd].buf.pop()) {
                shared_ptr<string> tmp_ptr = clients[fd].buf.front();
                //cout << "[SEND] " << *tmp_ptr << "to " << fd << endl;
                ssize_t data_out = send(fd, tmp_ptr->c_str(), tmp_ptr->size(), 0);
                if (data_out < 0) {
                    cerr << "send failed" << endl;
                    exit(1);
                }
                clients[fd].buf.pop();
            }
        }
    }
    close(server_socket);
}


class IO_buf {
public:
    string ibuf = string();
    string obuf = string();
};

void serv_recv(int fd, short ev, void *args){
    char buf[BUF_SIZE];
    ssize_t data_in = recv(fd, buf, BUF_SIZE, 0);
    if (data_in < 0) {
        cerr << "recv failed" << endl;
        exit(1);
    }
    if (data_in > 1) {
        buf[data_in] = '\0';
        ((IO_buf *) args)->ibuf += string(buf);
    }
}

void serv_send(int fd, short ev, void *args) {
    string *p_str = &((IO_buf *)args)->obuf;
    ssize_t data_out = send(fd, p_str->c_str(), p_str->size(), 0);
    if (data_out < 0) {
        cerr << "send failed";
        exit(1);
    }
    p_str->clear();
}

void cons_type(int fd, short ev, void *args){
    char buf[BUF_SIZE];
    ssize_t data_in = read(fd, buf, BUF_SIZE);
    if (data_in < 0) {
        cerr << "read failed" << endl;
        exit(1);
    }
    if (data_in > 1) {
        buf[data_in] = '\0';
        ((IO_buf *) args)->obuf += string(buf);
    }
    string *p_str = &((IO_buf *)args)->ibuf;
    if (p_str->size() > 0){
        cout << *p_str;
        p_str->clear();
    }
}

void client(uint16_t port, string ip) {
    // Client socket
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0) {
        cerr << "client socket error";
        exit(1);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip.c_str(), &addr.sin_addr);
    if (connect(client_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        cerr << "connect failed";
        exit(1);
    }
    IO_buf buf;
    // Init
    event_init();
    // Reading from server
    event serv_read_ev;
    event_set(&serv_read_ev, client_socket, EV_READ | EV_PERSIST, serv_recv, &buf);
    event_add(&serv_read_ev, NULL);
    // Reading from console
    event cons_read_ev;
    event_set(&cons_read_ev, STDIN_FILENO, EV_READ | EV_PERSIST, cons_type, &buf);
    event_add(&cons_read_ev, NULL);
    // Writing to server
    event serv_write_ev;
    event_set(&serv_write_ev, client_socket, EV_WRITE | EV_PERSIST, serv_send, &buf);
    event_add(&serv_write_ev, NULL);
    // Run
    event_dispatch();

    close(client_socket);
}

int main(int argc, char *argv[]) {
    uint16_t port = 12345;
    string ip = "127.0.0.1";
    string usage = "Usage: ./chat server|client [-p <port>] [-ip <ip_address>]\nDefault: port = 12345, ip = 127.0.0.1";
    int ind = 2;
    while (ind < argc) {
        if (!strcmp(argv[ind], "-p") && ++ind < argc) {
            stringstream ss(argv[ind], ios_base::in);
            ss >> port;
        } else if (!strcmp(argv[ind], "-ip") && ++ind < argc) {
            ip = string(argv[ind]);
        } else {
            cout << usage << endl;
            return 0;
        }
        ind++;
    }
    if (argc < 2) {
        cout << usage << endl;
        return 0;
    }
    if (!strcmp(argv[1], "server")) {
        server(port);
    } else if (!strcmp(argv[1], "client")) {
        client(port, ip);
    } else {
        cout << usage << endl;
    }
    return 0;
}