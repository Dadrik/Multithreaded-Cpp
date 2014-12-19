#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unistd.h>
#include <omp.h>

using namespace std;

int main(int argc, char *argv[]) {

    int stream[2];
    pipe(stream);
    if (!fork()) {
        dup2(stream[1], 2);
        close(stream[0]);
        close(stream[1]);
        char trace[] = "strace";
        argv[0] = trace;
        execvp("strace", argv);
        exit(0);
    } else {
        dup2(stream[0], 0);
        close(stream[0]);
        close(stream[1]);
    }
    vector<string> strace_res;
    unordered_map<string, int> map;

    #pragma omp parallel
    {
        string tmp;
        bool cycle = true;
        while (cycle) {
            #pragma omp critical
            {
                cycle = !!getline(cin, tmp);
            }
            if (!cycle) break;
            auto cut = tmp.find_first_of('(');
            if (cut == string::npos)
                continue;
            string res = tmp.substr(0, cut);
            #pragma omp critical
            {
                auto got = map.find(res);
                if (got != map.end())
                    got->second++;
                else
                    map.emplace(res, 1);
            }
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
        cout << i->first << ": " << i->second << endl;
    return 0;
}

