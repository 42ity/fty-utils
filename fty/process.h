#pragma once
#include <fcntl.h>
#include <fty/expected.h>
#include <fty/flags.h>
#include <iostream>
#include <poll.h>
#include <spawn.h>
#include <unistd.h>
#include <vector>
#include <wait.h>

namespace fty {

// =========================================================================================================================================

enum class Capture
{
    None = 1 << 0,
    Out  = 1 << 1,
    Err  = 1 << 2,
    In   = 1 << 3
};
ENABLE_FLAGS(Capture)

class Process
{
public:
    using Arguments = std::vector<std::string>;
    Process(const std::string& cmd, const Arguments& args = {}, Capture capture = Capture::Out | Capture::Err | Capture::In);
    ~Process();

    Expected<pid_t> run();
    Expected<int>   wait(int milliseconds = -1);

    std::string readAllStandardError(int milliseconds = -1);
    std::string readAllStandardOutput(int milliseconds = -1);
    bool        write(const std::string& cmd);
    void        closeWriteChannel();
    void        setEnvVar(const std::string& name, const std::string& val);
    void        addArgument(const std::string& arg);

    void interrupt();
    void kill();

    bool exists();

public:
    static Expected<int> run(const std::string& cmd, const Arguments& args, std::string& out, std::string& err);
    static Expected<int> run(const std::string& cmd, const Arguments& args, std::string& out);
    static Expected<int> run(const std::string& cmd, const Arguments& args);

private:
    std::string              m_cmd;
    std::vector<std::string> m_args;
    std::vector<std::string> m_environ;
    Capture                  m_capture;
    pid_t                    m_pid    = 0;
    int                      m_stdout = 0;
    int                      m_stderr = 0;
    int                      m_stdin  = 0;
};

// =========================================================================================================================================

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
        memset(s, 0, str.size() + 1);
        strncpy(s, str.c_str(), str.size());
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

inline Process::Process(const std::string& cmd, const Arguments& args, Capture capture)
    : m_cmd(cmd)
    , m_args(args)
    , m_capture(capture)
{
    for (int i = 0; environ[i]; ++i) {
        m_environ.push_back(environ[i]);
    }
}

inline Process::~Process()
{
    if (m_stdin) {
        closeWriteChannel();
    }
    if (m_pid) {
        kill();
        assert(true && "Process was running, killed...");
    }
}

inline Expected<pid_t> Process::run()
{
    int coutPipe[2];
    int cerrPipe[2];
    int cinPipe[2];

    posix_spawn_file_actions_t action;
    posix_spawn_file_actions_init(&action);

    if (isSet(m_capture, Capture::Out)) {
        if (pipe(coutPipe)) {
            return unexpected("pipe returned an error");
        }
        posix_spawn_file_actions_addclose(&action, coutPipe[0]);
        posix_spawn_file_actions_adddup2(&action, coutPipe[1], STDOUT_FILENO);
        // posix_spawn_file_actions_addclose(&action, coutPipe[1]);
    }

    if (isSet(m_capture, Capture::Err)) {
        if (pipe(cerrPipe)) {
            return unexpected("pipe returned an error");
        }
        posix_spawn_file_actions_addclose(&action, cerrPipe[0]);
        posix_spawn_file_actions_adddup2(&action, cerrPipe[1], STDERR_FILENO);
        // posix_spawn_file_actions_addclose(&action, cerrPipe[1]);
    }

    if (isSet(m_capture, Capture::In)) {
        if (pipe(cinPipe)) {
            return unexpected("pipe returned an error");
        }
        posix_spawn_file_actions_addclose(&action, cinPipe[1]);
        posix_spawn_file_actions_adddup2(&action, cinPipe[0], STDIN_FILENO);
        // posix_spawn_file_actions_addclose(&action, cinPipe[1]);
    }


    CharArray args(m_cmd, m_args);
    CharArray env(m_environ);

    if (posix_spawnp(&m_pid, m_cmd.data(), &action, nullptr, args.data(), env.data()) != 0) {
        return unexpected("posix_spawnp failed with error: {}", strerror(errno));
    }

    if (posix_spawn_file_actions_destroy(&action)) {
        return unexpected("posix_spawn_file_actions_destroy");
    }

    if (isSet(m_capture, Capture::Out)) {
        close(coutPipe[1]);
        m_stdout = coutPipe[0];
    }

    if (isSet(m_capture, Capture::Err)) {
        close(cerrPipe[1]);
        m_stderr = cerrPipe[0];
    }

    if (isSet(m_capture, Capture::In)) {
        close(cinPipe[0]);
        m_stdin = cinPipe[1];
    }

    return m_pid;
}

inline Expected<int> Process::wait(int milliseconds)
{
    closeWriteChannel();
    int status = 0;
    if (milliseconds >= 0) {
        sigset_t childMask, oldMask;
        sigemptyset(&childMask);
        sigaddset(&childMask, SIGCHLD);

        if (sigprocmask(SIG_BLOCK, &childMask, &oldMask) == -1) {
            return unexpected("sigprocmask failed: {}", strerror(errno));
        }

        timespec ts;
        ts.tv_sec  = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000;

        int res;
        do {
            res = sigtimedwait(&childMask, nullptr, &ts);
        } while (res == -1 && errno == EINTR);

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
        m_pid = 0;
        return status;
    }

    do {
        if (auto res = waitpid(m_pid, &status, WUNTRACED | WCONTINUED); res == -1) {
            return unexpected("waitpid");
        }

        if (WIFEXITED(status)) {
            m_pid = 0;
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            m_pid = 0;
            return WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            m_pid = 0;
            return WSTOPSIG(status);
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return unexpected("something wrong");
}

inline std::string readFromFd(int fd, int milliseconds, int maxretry = 2)
{
    std::array<char, 1024> buffer;
    std::string            output;

    timeval tv;
    if (milliseconds > 0) {
        tv.tv_sec  = milliseconds / 1000;
        tv.tv_usec = (milliseconds % 1000) * 1000;
    } else {
        tv.tv_sec  = 0;
        tv.tv_usec = 100 * 1000;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    int exit = 0;

    int o_flags = fcntl(fd, F_GETFL);
    int n_flags = o_flags | O_NONBLOCK;
    fcntl(fd, F_SETFL, n_flags);

    while(true) {
        if (int retval = select(fd+1, &readSet, nullptr, nullptr, &tv); retval > 0) {
            if (FD_ISSET(fd, &readSet)) {
                if (auto bytesRead = read(fd, &buffer[0], buffer.size()); bytesRead > 0) {
                    output += std::string(buffer.data(), size_t(bytesRead));
                } else {
                    if ((errno == EAGAIN || errno == EWOULDBLOCK) && exit < maxretry) {
                        ++exit;
                        continue;
                    }
                    break;
                }
            }
        } else if (retval == 0 && exit < maxretry) {
            ++exit;
            continue;
        } else {
            break;
        }
    }

    return output;
}

inline std::string Process::readAllStandardOutput(int milliseconds)
{
    return readFromFd(m_stdout, milliseconds);
}

inline std::string Process::readAllStandardError(int milliseconds)
{
    return readFromFd(m_stderr, milliseconds);
}

inline bool Process::write(const std::string& cmd)
{
    if (m_stdin) {
        auto ret = ::write(m_stdin, cmd.c_str(), cmd.size());
        fsync(m_stdin);
        return ret == ssize_t(cmd.size());
    }
    return false;
}

inline void Process::closeWriteChannel()
{
    if (m_stdin) {
        close(m_stdin);
        m_stdin = 0;
    }
}

inline void Process::setEnvVar(const std::string& name, const std::string& val)
{
    try {
        m_environ.push_back(fmt::format("{}={}", name, val));
    } catch (const fmt::format_error&) {
    }
}

inline void Process::addArgument(const std::string& arg)
{
    m_args.push_back(arg);
}


inline void Process::interrupt()
{
    if (m_pid) {
        ::kill(m_pid, SIGINT);
        int status;
        do {
            waitpid(m_pid, &status, WUNTRACED | WCONTINUED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status) && !WCOREDUMP(status));
        m_pid = 0;
    }
}

inline void Process::kill()
{
    if (m_pid) {
        ::kill(m_pid, SIGKILL);
        int status;
        do {
            waitpid(m_pid, &status, WUNTRACED | WCONTINUED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status) && !WCOREDUMP(status));
        m_pid = 0;
    }
}

inline bool Process::exists()
{
    if (m_pid) {
        return ::kill(m_pid, 0) == 0;
    }
    return false;
}

inline Expected<int> Process::run(const std::string& cmd, const Arguments& args, std::string& out, std::string& err)
{
    Process proc(cmd, args, Capture::Err | Capture::Out);
    if (auto ret = proc.run(); !ret) {
        return unexpected(ret.error());
    }
    out      = proc.readAllStandardOutput();
    err      = proc.readAllStandardError();
    auto ret = proc.wait();
    out      += proc.readAllStandardOutput();
    err      += proc.readAllStandardError();
    if (ret) {
        return *ret;
    } else {
        return unexpected(ret.error());
    }
}

inline Expected<int> Process::run(const std::string& cmd, const Arguments& args, std::string& out)
{
    Process proc(cmd, args, Capture::Out);
    if (auto ret = proc.run(); !ret) {
        return unexpected(ret.error());
    }
    out      = proc.readAllStandardOutput();
    auto ret = proc.wait();
    out      += proc.readAllStandardOutput();
    if (ret) {
        return *ret;
    } else {
        return unexpected(ret.error());
    }
}

inline Expected<int> Process::run(const std::string& cmd, const Arguments& args)
{
    Process proc(cmd, args, Capture::None);
    if (auto ret = proc.run(); !ret) {
        return unexpected(ret.error());
    }
    auto ret = proc.wait();
    if (ret) {
        return *ret;
    } else {
        return unexpected(ret.error());
    }
}


// =========================================================================================================================================
} // namespace fty
