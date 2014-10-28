#include <iostream>
#include <fstream>
#include <list>
#include <unistd.h>

using namespace std;

struct Request {
    string method;
    string path;
    list<string> data;
};

struct Respond {
    int status_code;
    list<string> data;
};

class Handler {
    string dir_path;
    Request request;
    Respond respond;
    void parse_request();
    void handle_request();
    void build_response();
public:
    Handler(string dir_path_): dir_path(dir_path_) {}
    int handle() {
        parse_request();
        handle_request();
        build_response();
        return 0;
    }
};

void Handler::parse_request() {
    string version;
    cin >> request.method >> request.path >> version;
    if (version.compare("HTTP/1.0")) {
        // Unsupported protocol
        cerr << "Unsupported protocol\n";
        exit(1);
    }
    if (!request.method.compare("POST")) {
        string tmp;
        // Skip headers
        getline(cin, tmp);
        do {
            getline(cin, tmp);
        } while (tmp.size() > 0);
        // Get payload
        while (getline(cin , tmp)) {
            request.data.push_back(tmp);
            //cout << tmp << endl;
        }
    }
}

void Handler::handle_request() {
    if (!request.method.compare("GET") || !request.method.compare("HEAD")) {
        ifstream ifs(dir_path + request.path, ifstream::in);
        if (!ifs.good()) {
            ifs.close();
            respond.status_code = 404;
        } else {
            if (!request.method.compare("GET")) {
                string tmp;
                while (getline(ifs, tmp)) {
                    respond.data.push_back(tmp);
                    //cout << tmp << endl;
                }
            }
            respond.status_code = 200;
            ifs.close();
        }
    } else if (!request.method.compare("POST")) {
        ofstream ofs(dir_path + request.path, ifstream::out | ifstream::app);
        if (!ofs.good()) {
            // Access denied
            ofs.close();
            cerr << "Access denied";
            exit(1);
        }
        for (auto iter = request.data.begin(); iter != request.data.end(); ++iter) {
            ofs << *iter << endl;
            //cout << *iter << endl;
        }
        respond.status_code = 200;
        ofs.close();
    } else {
        // Unknown method
        cerr << "Unknown method";
        exit(1);
    }
}

void Handler::build_response() {
    string status;
    switch (respond.status_code) {
        case 200: status = "OK"; break;
        case 404: status = "Not Found"; break;
    }
    cout << "HTTP/1.0 " << respond.status_code << " " << status << endl;
    if (respond.status_code == 200) {
        if (!request.method.compare("GET")) {
            cout << endl;
            for (auto iter = respond.data.begin(); iter != respond.data.end(); ++iter) {
                cout << *iter << endl;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    string dir_path = "./";

    int c;
    while ((c = getopt(argc, argv, "d:")) != -1)
        switch (c) {
            case 'd':
                dir_path = optarg;
                break;
            case '?':
                if (optopt == 'd')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }
    Handler handler(dir_path);
    handler.handle();
    return 0;
}