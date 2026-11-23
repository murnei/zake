#include "process.h"

#include <cerrno>
#include <cstring>

#include <sys/wait.h>
#include <unistd.h>

#include "error.h"

ProcessResult run_process(const std::vector<std::string>& args) {
    if (args.empty())
        throw ZakeError(ErrorCode::ProcessError, "empty command");

    int fds[2];
    if (pipe(fds) != 0)
        throw ZakeError(ErrorCode::ProcessError, std::strerror(errno));

    pid_t pid = fork();
    if (pid < 0) {
        std::string reason = std::strerror(errno);
        close(fds[0]);
        close(fds[1]);
        throw ZakeError(ErrorCode::ProcessError, reason);
    }

    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);

        std::vector<char*> argv;
        for (const auto& a : args)
            argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(fds[1]);

    std::string output;
    char buf[4096];
    for (;;) {
        ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n > 0)
            output.append(buf, static_cast<size_t>(n));
        else if (n == 0 || errno != EINTR)
            break;
    }
    close(fds[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            throw ZakeError(ErrorCode::ProcessError, std::strerror(errno));
    }

    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {code, std::move(output)};
}
