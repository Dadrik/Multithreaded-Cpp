#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <vector>
#include <algorithm>
#include <unistd.h>

using namespace std;

struct Request {
    string method;
    string path;
    vector<char> data;
};

struct Respond {
    int status_code;
    vector<char> data;
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
    void handle() {
        parse_request();
        handle_request();
        build_response();
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
        size_t cont_len = string::npos;
        // Skip headers except Content-Length
        // FIXME
        getline(cin, tmp);
        do {
            getline(cin, tmp);
            if (cont_len == string::npos && tmp.find("Content-Length") != string::npos) {
                string cont_len_str;
                stringstream ss(tmp, ios_base::in);
                ss >> cont_len_str >> cont_len;
            }
        } while (tmp.size() > 0);
        // Get payload
        // FIXME
        cin.unsetf(std::ios::skipws);
        istream_iterator<char> stream_begin(cin), stream_end;
        copy(stream_begin, stream_end, back_inserter(request.data));
    }
}

void Handler::handle_request() {
    if (!request.method.compare("GET") || !request.method.compare("HEAD")) {
        ifstream ifs(dir_path + request.path, ifstream::in | ifstream::binary);
        if (!ifs.good()) {
            ifs.close();
            respond.status_code = 404;
        } else {
            if (!request.method.compare("GET")) {
                ifs.unsetf(std::ios::skipws);
                istream_iterator<char> stream_begin(ifs), stream_end;
                copy(stream_begin, stream_end, back_inserter(respond.data));
            }
            respond.status_code = 200;
            ifs.close();
        }
    } else if (!request.method.compare("POST")) {
        ofstream ofs(dir_path + request.path, ifstream::out | ifstream::app | ifstream::binary);
        if (!ofs.good()) {
            // Access denied
            ofs.close();
            cerr << "Access denied";
            exit(1);
        }
        // FIXME
        /*
        ofs.unsetf(std::ios::skipws);
        ostream_iterator<char> stream_begin(ofs);
        copy(request.data.begin(), request.data.end(), stream_begin);
        */
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
            copy(respond.data.begin(), respond.data.end(), ostream_iterator<char>(cout));
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