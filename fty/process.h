#pragma once
#include <fty/expected.h>
#include <spawn.h>
#include <unistd.h>
#include <vector>
#include <wait.h>

namespace fty {

// =====================================================================================================================

class Process
{
public:
    Process(const std::string& cmd, const std::vector<std::string>& args = {});
    Expected<pid_t> run();
    Expected<int>   wait(int milliseconds = -1);

    std::string readAllStandardError();
    std::string readAllStandardOutput();
    void        setEnvVar(const std::string& name, const std::string& val);

    void interrupt();
    void kill();

    bool exists();

private:
    std::string              m_cmd;
    std::vector<std::string> m_args;
    std::vector<std::string> m_environ;
    pid_t                    m_pid    = 0;
    int                      m_stdout = 0;
    int                      m_stderr = 0;
};

// =====================================================================================================================

class CharArray
{
public:
    template <typename... Args>
    CharArray(const Args&... args)
    {
        add(args...);
        m_data.push_back(nullptr);
    }

    CharArray(const CharArray&) = delete;

    ~CharArray()
    {
        for (size_t i = 0; i < m_data.size(); i++) {
            delete[] m_data[i];
        }
    }

    template <typename... Args>
    void add(const std::string& arg, const Args&... args)
    {
        add(arg);
        add(args...);
    }

    void add(const std::string& str)
    {
        char* s = new char[str.size() + 1];
        strcpy(s, str.c_str());
        m_data.push_back(s);
    }

    void add(const std::vector<std::string>& vec)
    {
        for (const std::string& str : vec) {
            add(str);
        }
    }

    char** data()
    {
        return m_data.data();
    }

private:
    std::vector<char*> m_data;
};

inline Process::Process(const std::string& cmd, const std::vector<std::string>& args)
    : m_cmd(cmd)
    , m_args(args)
{
    for (int i = 0; environ[i]; ++i) {
        m_environ.push_back(environ[i]);
    }
}

inline Expected<pid_t> Process::run()
{
    int coutPipe[2];
    int cerrPipe[2];

    if (pipe(coutPipe) || pipe(cerrPipe)) {
        return unexpected("pipe returned an error");
    }

    posix_spawn_file_actions_t action;
    posix_spawn_file_actions_init(&action);
    posix_spawn_file_actions_addclose(&action, coutPipe[0]);
    posix_spawn_file_actions_addclose(&action, cerrPipe[0]);
    posix_spawn_file_actions_adddup2(&action, coutPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&action, cerrPipe[1], STDERR_FILENO);

    posix_spawn_file_actions_addclose(&action, coutPipe[1]);
    posix_spawn_file_actions_addclose(&action, cerrPipe[1]);

    CharArray args(m_cmd, m_args);
    CharArray env(m_environ);

    if (posix_spawnp(&m_pid, m_cmd.data(), &action, nullptr, args.data(), env.data()) != 0) {
        return unexpected("posix_spawnp failed with error: {}", strerror(errno));
    }

    if (auto res = posix_spawn_file_actions_destroy(&action)) {
        return unexpected("posix_spawn_file_actions_destroy");
    }

    close(coutPipe[1]);
    close(cerrPipe[1]);

    m_stdout = coutPipe[0];
    m_stderr = cerrPipe[0];

    return m_pid;
}

inline Expected<int> Process::wait(int milliseconds)
{
    int status = 0;
    if (milliseconds >= 0) {
        sigset_t childMask, oldMask;
        sigemptyset(&childMask);
        sigaddset(&childMask, SIGCHLD);

        if (sigprocmask(SIG_BLOCK, &childMask, &oldMask) == -1) {
            return unexpected("sigprocmask failed: {}", strerror(errno));
        }

        timespec ts;
        ts.tv_sec       = milliseconds / 1000;
        ts.tv_nsec      = (milliseconds % 1000) * 1000000;

        int res;
        do {
            res = sigtimedwait(&childMask, nullptr, &ts);
        } while(res == -1 && errno == EINTR);

        if (res == -1 && errno == EAGAIN) {
            return unexpected("timeout");
        } else if (res == -1) {
            return unexpected("sigtimedwait failed: {}", strerror(errno));
        }

        if (sigprocmask(SIG_SETMASK, &oldMask, nullptr) == -1) {
            return unexpected("sigprocmask failed: {}", strerror(errno));
        }

        pid_t childPid = waitpid(m_pid, &status, WNOHANG);
        if (childPid != m_pid) {
            if (childPid != -1) {
                return unexpected("Waiting for pid {}, got pid {} instead\n", m_pid, childPid);
            } else {
                return unexpected("waitpid failed: {}", strerror(errno));
            }
        }
        return status;
    }

    do {
        if (auto res = waitpid(m_pid, &status, WUNTRACED | WCONTINUED); res == -1) {
            return unexpected("waitpid");
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            return WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            return WSTOPSIG(status);
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return unexpected("something wrong");
}

inline std::string Process::readAllStandardOutput()
{
    std::array<char, 1024> buffer;
    std::string            output;

    ssize_t bytesRead;
    while ((bytesRead = read(m_stdout, &buffer[0], buffer.size())) > 0) {
        output += std::string(buffer.data(), size_t(bytesRead));
    }
    return output;
}

inline std::string Process::readAllStandardError()
{
    std::array<char, 1024> buffer;
    std::string            output;

    ssize_t bytesRead;
    while ((bytesRead = read(m_stderr, &buffer[0], buffer.size())) > 0) {
        output += std::string(buffer.data(), size_t(bytesRead));
    }
    return output;
}

inline void Process::setEnvVar(const std::string& name, const std::string& val)
{
    m_environ.push_back(fmt::format("{}={}", name, val));
}


inline void Process::interrupt()
{
    ::kill(m_pid, SIGINT);
}

inline void Process::kill()
{
    ::kill(m_pid, SIGKILL);
}

inline bool Process::exists()
{
    return (::kill(m_pid, 0) == 0);
}

// =====================================================================================================================
} // namespace fty
