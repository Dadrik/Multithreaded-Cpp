#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <unistd.h>
#include <omp.h>

using namespace std;

int main(int argc, char *argv[]) {
    // Run forked strace
    int stream[2];
    pipe(stream);
    if (!fork()) {
        // Write to pipe
        dup2(stream[1], 2);
        close(stream[0]);
        close(stream[1]);
        char trace[] = "strace";
        argv[0] = trace;
        execvp(trace, argv);
        exit(0);
    } else {
        // Read from pipe
        dup2(stream[0], 0);
        close(stream[0]);
        close(stream[1]);
    }

    // Storage
    unordered_map<string, int> syscall_usage;

    // Parallel handle
    #pragma omp parallel
    {
        string strace_output_line;
        bool cycle_condition = true;
        while (cycle_condition) {
            #pragma omp critical
            {
                cycle_condition = !!getline(cin, strace_output_line);
            }
            if (!cycle_condition) break;
            auto border = strace_output_line.find_first_of('(');
            if (border == string::npos)
                continue;
            string system_call = strace_output_line.substr(0, border);
            #pragma omp critical
            {
                auto search_result = syscall_usage.find(system_call);
                if (search_result != syscall_usage.end())
                    search_result->second++;
                else
                    syscall_usage.emplace(system_call, 1);
            }
        }
    }

    // Output
    cout << endl << "System calls usage:" << endl;
    for (auto i = syscall_usage.begin(); i != syscall_usage.end(); i++)
        cout << i->first << ": " << i->second << endl;

    return 0;
}

