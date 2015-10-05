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


class Client_info {
    string ip;
    queue<shared_ptr<string>> messages;
public:
    Client_info(string _ip = "unknown")  {
        ip = _ip;
    };
    string GetIP() {
        return ip;
    }
    void AddMessage(shared_ptr<string> message) {
        messages.push(message);
    }
    string GetMessage() {
        if (!messages.empty()) {
            return *messages.front();
        } else
            return "";
    }
    void DeleteMessage() {
        messages.pop();
    }
};

class Client_list {
    unordered_map<int, Client_info> clients;
public:
    void AddClient(int socket, string ip) {
        Client_info client(ip);
        clients.emplace(socket, client);
    }
    void DeleteClient(int socket) {
        clients.erase(socket);
    }
    void PutMessage(int socket, string message) {
        shared_ptr<string> formatted_message_ptr(new string("[" + clients[socket].GetIP() + "] " + message));
        //cout << "[RECV] " << *ptr << endl;
        for (auto client_iter = clients.begin(); client_iter != clients.end(); ++client_iter) {
            if (client_iter->first != socket) {
                client_iter->second.AddMessage(formatted_message_ptr);
            }
        }
    }
    string GetMessage(int socket) {
        return clients[socket].GetMessage();
    }
    void DeleteMessage(int socket) {
        clients[socket].DeleteMessage();
    }
};


class Server {
private:
    // Clients and buffers
    int server_socket;
    int epoll_fd;
    epoll_event event;
    Client_list clients;

    void AddClient(epoll_event event) {
        // Accept new client
        sockaddr_in client_addr;
        socklen_t socklen;
        int connect_sock = accept(server_socket, (struct sockaddr *) &client_addr, &socklen);
        if (connect_sock < 0) {
            cerr << "accept failed" << endl;
            exit(1);
        }

        cout << "[ACC] socket = " << connect_sock << endl;

        clients.AddClient(connect_sock, string(inet_ntoa(client_addr.sin_addr)));

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLHUP;
        ev.data.fd = connect_sock;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connect_sock, &ev) == -1) {
            cerr << "epoll_ctl failed" << endl;
            exit(1);
        }
    }

    void DeleteClient(epoll_event) {
        int fd = event.data.fd;
        cout << "[CLOSE] socket " << fd << endl;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event) != 0) {
            cerr << "epoll_ctl failed" << endl;
            exit(1);
        }
        clients.DeleteClient(fd);
        close(fd);
    }

    void ReceiveData(epoll_event event) {
        int fd = event.data.fd;
        char buf[BUF_SIZE];
        ssize_t data_in = recv(fd, buf, BUF_SIZE, 0);
        if (data_in < 0) {
            cerr << "recv failed" << endl;
            exit(1);
        } else if (data_in == 0) {
            DeleteClient(event);
            return;
        }
        buf[data_in] = '\0';
        string message = string(buf);
        clients.PutMessage(fd, message);
    }

    void SendData(epoll_event event) {
        int fd = event.data.fd;
        //for ( ; !clients[fd].buf.empty(); clients[fd].buf.pop()) {
        string message = clients.GetMessage(fd);
        //cout << "[SEND] " << *tmp_ptr << "to " << fd << endl;
        if (message != "") {
            ssize_t data_out = send(fd, message.c_str(), message.size(), 0);
            if (data_out < 0) {
                cerr << "send failed" << endl;
                exit(1);
            }
            clients.DeleteMessage(fd);
        }
    }
public:
    Server(uint16_t port) {
        // Socket init
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket < 0) {
            cerr << "server socket error" << endl;
            exit(1);
        }
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(server_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            cerr << "bind failed" << endl;
            exit(1);
        }
        if (listen(server_socket, 10) < 0) {
            cerr << "listen failed";
            exit(1);
        }

        // EPOLL init
        epoll_fd = epoll_create1(0);
        if (epoll_fd < 0) {
            cerr << "epoll_create failed" << endl;
            exit(1);
        }
    }

    void Run() {
        epoll_event ev;
        ev.data.fd = server_socket;
        ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
            cerr << "epoll_ctl failed" << endl;
            exit(1);
        }

        while (true) {
            int nfds = epoll_wait(epoll_fd, &event, 1, -1);
            if (nfds < 0) {
                cerr << "epoll_wait failed" << endl;
                exit(1);
            }
            // Event handling
            for (int n = 0; n < nfds; n++) {
                if (event.data.fd == server_socket) { // Accept client
                    AddClient(event);
                } else if (event.events & EPOLLIN) { // Read data from client
                    ReceiveData(event);
                } else if (event.events & EPOLLHUP) { // Delete client
                    DeleteClient(event);
                } else if (event.events & EPOLLOUT) { // Write data to client
                    SendData(event);
                }
            }
        }
    }
    ~Server() {
        close(server_socket);
    }
};


class IO_buffer {
public:
    string ibuf = string();
    string obuf = string();
};


class Client {
private:
    int client_socket;
    IO_buffer buffer;

    static void serv_recv(int fd, short ev, void *args){
        char buf[BUF_SIZE];
        ssize_t data_in = recv(fd, buf, BUF_SIZE, 0);
        if (data_in < 0) {
            cerr << "recv failed" << endl;
            exit(1);
        }
        if (data_in > 1) {
            buf[data_in] = '\0';
            ((IO_buffer *) args)->ibuf += string(buf);
        }
    }

    static void serv_send(int fd, short ev, void *args) {
        string *p_str = &((IO_buffer *)args)->obuf;
        ssize_t data_out = send(fd, p_str->c_str(), p_str->size(), 0);
        if (data_out < 0) {
            cerr << "send failed" << endl;
            exit(1);
        }
        p_str->clear();
    }

    static void cons_type(int fd, short ev, void *args){
        char buf[BUF_SIZE];
        ssize_t data_in = read(fd, buf, BUF_SIZE);
        if (data_in < 0) {
            cerr << "read failed" << endl;
            exit(1);
        }
        if (data_in > 1) {
            buf[data_in] = '\0';
            ((IO_buffer *) args)->obuf += string(buf);
        }
        string *p_str = &((IO_buffer *)args)->ibuf;
        if (p_str->size() > 0){
            cout << *p_str;
            p_str->clear();
        }
    }
public:
    Client(uint16_t port, string ip) {
        // Client socket init
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket < 0) {
            cerr << "client socket error" << endl;
            exit(1);
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_aton(ip.c_str(), &addr.sin_addr);
        if (connect(client_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            cerr << "connect failed" << endl;
            exit(1);
        }
    }

    void Run() {
        event_init();
        // Reading from server
        event serv_read_ev;
        event_set(&serv_read_ev, client_socket, EV_READ | EV_PERSIST, serv_recv, &buffer);
        event_add(&serv_read_ev, NULL);
        // Reading from console
        event cons_read_ev;
        event_set(&cons_read_ev, STDIN_FILENO, EV_READ | EV_PERSIST, cons_type, &buffer);
        event_add(&cons_read_ev, NULL);
        // Writing to server
        event serv_write_ev;
        event_set(&serv_write_ev, client_socket, EV_WRITE | EV_PERSIST, serv_send, &buffer);
        event_add(&serv_write_ev, NULL);
        // Run
        event_dispatch();
    }

    ~Client() {
        close(client_socket);
    }
};


int main(int argc, char *argv[]) {
    uint16_t port = 12345;
    string ip = "127.0.0.1";
    string const usage = "Usage: ./chat server|client [-p <port>] [-ip <ip_address>]\n"
            "Default: port = 12345, ip = 127.0.0.1";
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
        Server server(port);
        server.Run();
    } else if (!strcmp(argv[1], "client")) {
        Client client(port, ip);
        client.Run();
    } else {
        cout << usage << endl;
    }
    return 0;
}
