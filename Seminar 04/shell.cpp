#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>

using namespace std;

/*
    1) basic_exec ($ bin -> PID = ... ; exec ; PID = ... exit = ...)
    2) background_exec ($ bin & -> PID = ... ; <background exec> ; (after any process) PID = ... exit = ...)
    3) write_to_file ($ bin [&] > file -> --//-- to file)
    4) basic_pipe
*/

struct Command {
    vector<string> args;
};

class Pipeline {
    vector<Command> pp_cmd;
    bool background;
    string file = string();
public:
    Pipeline(string &cmd_line);
    int execute();
};

Pipeline::Pipeline(string &cmd_line) {
    size_t index = cmd_line.find('&');
    if (index != string::npos) {
        background = true;
        cmd_line.resize(index);
    } else {
        background = false;
    }
    index = cmd_line.find('>');
    if (index != string::npos) {
        stringstream file_stream(cmd_line.substr(index + 1, cmd_line.size() - index), ios_base::in);
        file_stream >> file;
        cmd_line.resize(index);
    }
    while (!cmd_line.empty()){
        index = cmd_line.find_first_of('|');
        string sub_cmd;
        if (index != string::npos) {
            sub_cmd = cmd_line.substr(0, index);
            cmd_line.erase(0, index + 1);
        } else {
            sub_cmd = cmd_line;
            cmd_line.clear();
        }
        stringstream ss(sub_cmd, ios_base::in);
        Command cmd;
        string tmp;
        do {
            tmp.clear();
            ss >> tmp;
            if (!tmp.empty()) {
                cmd.args.push_back(tmp);
            }
        } while (!ss.eof());
        pp_cmd.push_back(cmd);
    }
}

int Pipeline::execute() {
    pid_t pid;
    int p[2][2];
    vector<pid_t> to_wait;
    for (int i = 0; i < pp_cmd.size(); i++) {
        if (i < pp_cmd.size() - 1)
            pipe(p[i % 2]);
        if ((pid = fork())) {
            if (i > 0) {
                close(p[(i + 1) % 2][0]);
                close(p[(i + 1) % 2][1]);
            }
            printf("[START] PID = %d %s\n", pid, pp_cmd[i].args[0].c_str());
            if (!background)
                to_wait.push_back(pid);
        } else {
            if (i > 0) {
                dup2(p[(i + 1) % 2][0], 0);
                close(p[(i + 1) % 2][0]);
                close(p[(i + 1) % 2][1]);
            }
            if (i < pp_cmd.size() - 1) {
                dup2(p[i % 2][1], 1);
                close(p[i % 2][0]);
                close(p[i % 2][1]);
            } else if (!file.empty()) {
                int fd = open(file.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
                dup2(fd, 1);
                close(fd);
            }
            // vector<string> to char**
            char **argv = new char *[pp_cmd[i].args.size() + 1];
            for (size_t j = 0; j < pp_cmd[i].args.size(); j++) {
                argv[j] = new char[pp_cmd[i].args[j].size() + 1];
                strcpy(argv[j], pp_cmd[i].args[j].c_str());
            }
            argv[pp_cmd[i].args.size()] = NULL;
            // exec
            execvp(argv[0], argv);
            // free char **
            for (size_t j = 0; j < pp_cmd[i].args.size(); j++) {
                delete[] argv[j];
            }
            delete[] argv;
            // exit
            exit(0);
        }
    }
    int counter = pp_cmd.size();
    if (!background) {
        int stat;
        while (!to_wait.empty()) {
            pid = waitpid(-1, &stat, 0);
            counter--;
            printf("[EXIT] PID = %d exit = %d\n", pid, stat);
            for (auto iter = to_wait.begin(); iter != to_wait.end(); ++iter) {
                if (*iter == pid) {
                    to_wait.erase(iter);
                    break;
                }
            }
        }
    }
    return counter;
}

int main() {
    string cmd;
    int active = 0;
    while (true) {
        getline(cin, cmd);
        if (cmd == "exit") {
            exit(0);
        }
        Pipeline cmd_pipe(cmd);
        active += cmd_pipe.execute();
        if (active > 0) {
            int stat;
            pid_t pid = waitpid(-1, &stat, WNOHANG);
            if (pid) {
                printf("[EXIT] PID = %d exit = %d\n", pid, stat);
                active--;
            }
        }
    }
    return 0;
}