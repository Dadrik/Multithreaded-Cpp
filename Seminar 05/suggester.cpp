#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <fstream>

using namespace std;

#define N_WORKERS 4
#define S_WORDS 32
#define N_WORDS 10

ssize_t sock_fd_write(int sock, int fd) {
    ssize_t size;
    struct msghdr msg;
    union {
        struct cmsghdr cmsghdr;
        char control[CMSG_SPACE(sizeof(int))];
    } cmsgu;
    struct cmsghdr  *cmsg;

    char buf[] = "#";
    iovec iov;
    iov.iov_base = buf;
    iov.iov_len = strlen(buf) + 1;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    *((int *) CMSG_DATA(cmsg)) = fd;

    size = sendmsg(sock, &msg, 0);

    if (size < 0)
        perror("sendmsg");
    return size;
}

int sock_fd_read(int sock) {
    ssize_t size;
    struct msghdr msg;
    union {
        struct cmsghdr cmsghdr;
        char control[CMSG_SPACE(sizeof(int))];
    } cmsgu;
    struct cmsghdr *cmsg;

    char buf[] = "#";
    iovec iov;
    iov.iov_base = buf;
    iov.iov_len = strlen(buf) + 1;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);
    size = recvmsg(sock, &msg, 0);
    if (size < 0) {
        perror("recvmsg");
        exit(1);
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int)))
        return *((int *) CMSG_DATA(cmsg));
}

void worker(int master_socket) {
    while (true) {
        char buf[S_WORDS];
        char response[(S_WORDS + 1) * N_WORDS] = "";
        // Get client request
        int client_socket = sock_fd_read(master_socket);
        ssize_t req_size = recv(client_socket, buf, S_WORDS, 0);
        if (req_size < 0) {
            cerr << "recv failed\n";
            shutdown(client_socket, SHUT_RDWR);
            continue;
        }
        if (buf[req_size - 1] == '\n')
            buf[req_size - 1] = 0;
        else
            buf[req_size] = 0;
        const int len = strlen(buf);
        // Search
        ifstream ifs("words.txt", ifstream::in);
        int counter = 0;
        while (ifs.good() && counter < N_WORDS) {
            string tmp;
            ifs >> tmp;
            if (strncmp(buf, tmp.c_str(), len) == 0) {
                strcat(response, tmp.append("\n").data());
                counter++;
            }
        }
        ifs.close();
        // Send response
        ssize_t resp_size = send(client_socket, response, strlen(response), 0);
        if (resp_size < strlen(response)) {
            cerr << "recv failed\n";
            shutdown(client_socket, SHUT_RDWR);
            continue;
        }
        // Handle client
        shutdown(client_socket, SHUT_RDWR);
    }
}

void master(uint16_t port = 12345) {
    // Init workers
    int worker_socket[N_WORKERS];
    for (int i = 0; i < N_WORKERS; i++) {
        int socket_pair[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair);
        if (!fork()) {
            close(socket_pair[0]);
            worker(socket_pair[1]);
            exit(0);
        } else {
            close(socket_pair[1]);
            worker_socket[i] = socket_pair[0];
        }
    }

    // Init server socket
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

    // Process clients
    int worker = 0;
    while (true) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            cerr << "accept failed" << endl;
            continue;
        }
        sock_fd_write(worker_socket[worker], client_socket);
        worker = (worker + 1) % N_WORKERS;
    }
}

int main() {
    master();
    return 0;
}