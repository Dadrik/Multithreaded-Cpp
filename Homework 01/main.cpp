#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <algorithm>
#include <unistd.h>

using namespace std;

#define BUF_SIZE 1024

void handle(string dir) {
    string version, path, method;
    while (cin >> method >> path >> version) {
        if (!method.compare("GET") || !method.compare("HEAD")) {
            ifstream ifs(dir + path, ifstream::in | ifstream::binary);
            if (!ifs.good()) {
                cout << "HTTP/1.0 " << "404 " << "Not Found" << endl;
            } else {
                cout << "HTTP/1.0 " << "200 " << "OK" << endl << endl;
                if (!method.compare("GET")) {
                    char buf[BUF_SIZE];
                    while (ifs.read(buf, BUF_SIZE)) {
                        cout.write(buf, ifs.gcount());
                    }
                    if (ifs.eof()) {
                        if (ifs.gcount() > 0) {
                            cout.write(buf, ifs.gcount());
                        }
                    }
                }
                ifs.close();
            }
        } else if (!method.compare("POST")) {
            ofstream ofs(dir + path, ofstream::out | ofstream::app | ofstream::binary);
            if (!ofs.good()) {
                cerr << "Access denied";
                exit(1);
            }
            cout << "HTTP/1.0 " << "200 " << "OK" << endl;
            ofs.close();
        } else {
            cerr << "Unknown method";
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    string dir_path = ".";

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
    handle(dir_path);
    return 0;
}
