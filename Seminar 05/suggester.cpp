#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>

#include <iostream>
#include <fstream>

using namespace std;

#define N_WORKERS 4
#define S_WORDS 32
#define N_WORDS 10
#define N_CACHED 10
#define TTL 1

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

struct cached_elem {
    char ttl = 0;
    char request[S_WORDS];
    char respond[(S_WORDS + 1) * N_WORDS];
};

class Mutex {
    int mutex_id;
public:
    Mutex() {
        key_t ipc_key = ftok("/ets/passwd", '0');
        mutex_id = semget(ipc_key, 1, IPC_CREAT | 0777);
        semctl(mutex_id, 0, SETVAL, 1);
    }
    ~Mutex() {
        semctl(mutex_id, 0, IPC_RMID);
    }
    int lock() {
        struct sembuf buf = {0, -1, 0};
        return semop(mutex_id, &buf, 1);
    }
    int unlock() {
        struct sembuf buf = {0, 1, 0};
        return semop(mutex_id, &buf, 1);
    }
};

class Memory {
    int mem_id;
    cached_elem *cache;
public:
    Memory() {
        key_t ipc_key = ftok("/ets/passwd", '0');
        mem_id = shmget(ipc_key, sizeof(cached_elem) * N_CACHED, IPC_CREAT | 0777);
        cache = (struct cached_elem *) shmat(mem_id, NULL, 0);
        bzero(cache, sizeof(cached_elem) * N_CACHED);
    }
    ~Memory() {
        shmdt(cache);
        shmctl(mem_id, IPC_RMID, NULL);
    }
    cached_elem *get() {
        return cache;
    }
};

class Shared_cache {
private:
    Mutex mutex;
    Memory memory;
public:
    char *get_resp(char *request) {
        cached_elem *cache = memory.get();
        // Search in cache
        mutex.lock();
        int write_ind = 0;
        char min_ttl = cache[0].ttl;
        for (int index = 0; index < N_CACHED; index++) {
            if (cache[index].ttl == 0) {
                min_ttl = 0;
                write_ind = index;
                continue;
            }
            else {
                if (strcmp(cache[index].request, request) == 0) {
                    char *response = (char *) malloc ((S_WORDS + 1) * N_WORDS);
                    strcpy(response, cache[index].respond);
                    mutex.unlock();
                    return response;
                } else if (cache[index].ttl < min_ttl) {
                    min_ttl = cache[index].ttl;
                    write_ind = index;
                }
            }
        }
        cache[write_ind].ttl = TTL;
        strcpy(cache[write_ind].request, request);
        cache[write_ind].respond[0] = '\0';
        // Search in file
        const size_t len = strlen(request);
        ifstream ifs("words.txt", ifstream::in);
        int counter = 0;
        while (ifs.good() && (counter < N_WORDS)) {
            string tmp;
            ifs >> tmp;
            if (strncmp(request, tmp.c_str(), len) == 0) {
                strcat(cache[write_ind].respond, tmp.append("\n").data());
                counter++;
            }
        }
        ifs.close();
        // Make response
        char *response = (char *) malloc ((S_WORDS + 1) * N_WORDS);
        strcpy(response, cache[write_ind].respond);
        mutex.unlock();
        return response;
    }
    void ttl_decr() {
        while(true) {
            mutex.lock();
            cached_elem *cache = memory.get();
            for (int i = 0; i < N_CACHED; i++) {
                if (cache[i].ttl > 0) {
                    cache[i].ttl--;
                }
            }
            mutex.unlock();
            sleep(1);
        }
    }
};

void ttl_handler(Shared_cache *cache) {
    cache->ttl_decr();
}

void worker(int master_socket, Shared_cache *cache) {
    while (true) {
        char request[S_WORDS];
        // Get client request
        int client_socket = sock_fd_read(master_socket);
        ssize_t req_size = recv(client_socket, request, S_WORDS, 0);
        if (req_size < 0) {
            cerr << "recv failed\n";
            shutdown(client_socket, SHUT_RDWR);
            continue;
        }
        if (request[req_size - 1] == '\n')
            request[req_size - 1] = 0;
        else
            request[req_size] = 0;
        // Get responde
        char *response = cache->get_resp(request);
        // Send response
        ssize_t resp_size = send(client_socket, response, strlen(response), 0);
        free(response);
        // Handle client
        shutdown(client_socket, SHUT_RDWR);
    }
}

void master(uint16_t port) {
    // Init cache
    Shared_cache cache;
    // Init workers
    if (!fork()) {
        ttl_handler(&cache);
        exit(0);
    }
    int worker_socket[N_WORKERS];
    for (int i = 0; i < N_WORKERS; i++) {
        int socket_pair[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair);
        if (!fork()) {
            close(socket_pair[0]);
            worker(socket_pair[1], &cache);
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

int main(int argc, char *argv[]) {
    uint16_t port = 12345;
    master(port);
    return 0;
}