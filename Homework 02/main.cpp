#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <queue>
#include <thread>
#include <mutex>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#define BUF_SIZE 4096

using namespace std;

mutex Mutex;
queue<int> sockets;

class Worker {
    string dir;
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
    long get_file_size(string filename) {
        struct stat stat_buf;
        int rc = stat(filename.data(), &stat_buf);
        return rc == 0 ? size_t(stat_buf.st_size) : -1;
    }
    bool end_with(const string str, const string suffix) {
        return (str.size() >= suffix.size()) &&
                (str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
    }
    string lowercase(const string str) {
        string result = "";
        for (auto i = str.begin(); i != str.end(); ++i)
            result += tolower(*i);
        return result;
    }
    string get_content_type(string filename) {
        string lc_file = lowercase(filename);
        if (end_with(lc_file, ".html")) {
            return "text/html";
        } else if (end_with(lc_file, ".jpg") || end_with(lc_file, ".jpeg")) {
            return "image/jpeg";
        } else {
            return "text/plain";
        }
    }
public:
    Worker(string dir_):dir(dir_) {}
    void operator()() {
        while (true) {
            char request[BUF_SIZE];
            // Get client request
            int client_socket = get_socket();
            while(true) {
                ssize_t req_size = recv(client_socket, request, BUF_SIZE, 0);
                if (req_size <= 0) {
                    shutdown(client_socket, SHUT_RDWR);
                    break;
                }
                string version, path, method;
                stringstream req(request, ios_base::in);
                req >> method >> path >> version;
                // GET and HEAD
                if (!method.compare("GET") || !method.compare("HEAD")) {
                    // Headers
                    long size = get_file_size(dir + path);
                    if (size < 0) {
                        string header = "HTTP/1.0 404 Not Found\n";
                        ssize_t resp_size = send(client_socket, header.data(), header.size(), 0);
                        continue;
                    }
                    string header = "HTTP/1.0 200 OK\n";
                    if (method.compare("HEAD") == 0) {
                        ssize_t resp_size = send(client_socket, header.data(), header.size(), 0);
                        if (resp_size < header.size()) {
                            shutdown(client_socket, SHUT_RDWR);
                            break;
                        }
                    } else {
                        header += "Content-Length: " + to_string(size) + "\n";
                        header += "Content-Type: " + get_content_type(path) + "\n\n";
                        ssize_t resp_size = send(client_socket, header.data(), header.size(), 0);
                        if (resp_size < header.size()) {
                            shutdown(client_socket, SHUT_RDWR);
                            break;
                        }
                        // Content
                        ifstream ifs(dir + path, ifstream::in | ifstream::binary);
                        char buf[BUF_SIZE];
                        while (ifs.read(buf, BUF_SIZE)) {
                            ssize_t resp_size = send(client_socket, buf, size_t(ifs.gcount()), 0);
                            if (resp_size < size_t(ifs.gcount())) {
                                shutdown(client_socket, SHUT_RDWR);
                                break;
                            }
                        }
                        if (ifs.eof()) {
                            if (ifs.gcount() > 0) {
                                ssize_t resp_size = send(client_socket, buf, size_t(ifs.gcount()), 0);
                                if (resp_size < size_t(ifs.gcount()))
                                    shutdown(client_socket, SHUT_RDWR);
                            }
                        }
                        ifs.close();
                    }
                // POST
                } else if (!method.compare("POST")) {
                    ofstream ofs(dir + path, ofstream::out | ofstream::app | ofstream::binary);
                    if (!ofs.good()) {
                        cerr << "Access denied";
                        exit(1);
                    }
                    string header = "HTTP/1.0 200 OK\n";
                    ssize_t resp_size = send(client_socket, header.data(), header.size(), 0);
                    ofs.close();
                } else {
                    cerr << "Unknown method";
                    shutdown(client_socket, SHUT_RDWR);
                    break;
                }
            }
        }
    }
};

void master(string dir, string host, uint16_t port, int n_worker) {
    //Shared_cache cache;
    vector<Worker> worker(n_worker, Worker(dir));
    thread threads[n_worker];
    // Init workers
    for (int i = 0; i < n_worker; i++) {
        threads[i] = thread(ref(worker[i]));
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
    while (true) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            cerr << "accept failed" << endl;
            continue;
        }
        Mutex.lock();
        sockets.push(client_socket);
        Mutex.unlock();
    }
};

int main(int argc, char *argv[]) {
    string dir_path = ".";
    string host = "127.0.0.1";
    uint16_t port = 12345;
    int n_worker = 1;
    int c;
    while ((c = getopt(argc, argv, "dhpw:")) != -1) {
        stringstream ss(optarg, ios_base::in);
        switch (c) {
            case 'd':
                dir_path = optarg;
                break;
            case 'h':
                host = optarg;
                break;
            case 'p':
                ss >> port;
                break;
            case 'w':
                ss >> n_worker;
                break;
            case '?':
                if (optopt == 'd' || optopt == 'h' || optopt == 'p' || optopt == 'w')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }
    }
    master(dir_path, host, port, n_worker);
    return 0;
}
