#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <thread>
#include <mutex>

using namespace std;

#define N_WORKERS 4 // number of threads
#define S_WORDS 32 // max length of words
#define N_WORDS 10 // max number of words in response
#define TTL 20 // time to erase response from cache (seconds)

struct cached_elem {
    char ttl;
    char respond[(S_WORDS + 1) * N_WORDS];
};

class Shared_cache {
    mutex Mutex;
    unordered_map<string, cached_elem> cache;
public:
    char *get_response(char *request) {
        int index;
        char *response = (char *) malloc ((S_WORDS + 1) * N_WORDS);
        Mutex.lock();
        // Search in cache
        auto res = cache.find(string(request));
        if (res != cache.end()) {
            strcpy(response, res->second.respond);
            res->second.ttl = TTL;
            Mutex.unlock();
            return response;
        }
        // Search in file
        response[0] = '\0';
        const size_t len = strlen(request);
        ifstream ifs("words.txt", ifstream::in);
        int counter = 0;
        while (ifs.good() && (counter < N_WORDS)) {
            string tmp;
            ifs >> tmp;
            if (strncmp(request, tmp.c_str(), len) == 0) {
                strcat(response, tmp.append("\n").data());
                counter++;
            }
        }
        ifs.close();
        cached_elem new_elem;
        new_elem.ttl = TTL;
        strcpy(new_elem.respond, response);
        cache.insert(pair<string, cached_elem>(string(request), new_elem));
        Mutex.unlock();
        // Make response
        return response;
    }
    void ttl_decr() {
        Mutex.lock();
        auto iter = cache.begin();
        while (iter != cache.end()) {
            iter->second.ttl--;
            if (iter->second.ttl == 0) {
                iter = cache.erase(iter);
            } else {
                ++iter;
            }
        }
        Mutex.unlock();
    }
} cache;

class TTL_handler {
public:
    void operator()() {
        while (true) {
            cache.ttl_decr();
            sleep(1);
        }
    }
};

class Worker {
    mutex Mutex;
    queue<int> sockets;
    int get_socket() {
        while (true) {
            Mutex.lock();
            if (sockets.size() == 0) {
                Mutex.unlock();
                usleep(100);
            } else {
                int res = sockets.front();
                sockets.pop();
                Mutex.unlock();
                return res;
            }
        }
    }
public:
    void add_socket(int socket) {
        Mutex.lock();
        sockets.push(socket);
        Mutex.unlock();
    }
    void operator()() {
        while (true) {
            char request[S_WORDS];
            // Get client request
            int client_socket = get_socket();
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
            char *response = cache.get_response(request);
            // Send response
            ssize_t resp_size = send(client_socket, response, strlen(response), 0);
            free(response);
            // Handle client
            shutdown(client_socket, SHUT_RDWR);
        }
    }
};

void master(uint16_t port) {
    // Init cache
    //Shared_cache cache;
    Worker worker[N_WORKERS];
    thread threads[N_WORKERS];
    // Init workers
    for (int i = 0; i < N_WORKERS; i++) {
        threads[i] = thread(ref(worker[i]));
    }
    TTL_handler ttl_handler;
    thread ttl_worker(ttl_handler);

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
    int worker_ind = 0;
    while (true) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            cerr << "accept failed" << endl;
            continue;
        }
        worker[worker_ind].add_socket(client_socket);
        worker_ind = (worker_ind + 1) % N_WORKERS;
    }
}

int main(int argc, char *argv[]) {
    uint16_t port = 12345;
    master(port);
    return 0;
}
