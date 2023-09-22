/*  ========================================================================
    Copyright (C) 2021 Eaton
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ========================================================================
*/
#pragma once
#include <chrono>
#include <fcntl.h>
#include <fty/expected.h>
#include <fty/flags.h>
#include <iostream>
#include <atomic>
#include <sstream>
#include <mutex>
#include <limits>
#include <poll.h>
#include <spawn.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <wait.h>
#include <string.h>

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
    //unlimited wait, is limited to uint64_t max value (looks reasonable as our universe should have vanished by then)
    inline static const uint64_t UNLIMITED = (std::numeric_limits<uint64_t>::max()-1);

    using Arguments = std::vector<std::string>;
    Process(const std::string& cmd, const Arguments& args = {}, Capture capture = Capture::Out | Capture::Err | Capture::In);
    ~Process();

    Expected<pid_t> run();


    Expected<int>   wait(uint64_t timeoutMs = UNLIMITED, uint32_t waitCycleDurationMs = 100);

    std::string readAllStandardError(uint64_t timeoutMs = 100);
    std::string readAllStandardOutput(uint64_t timeoutMs = 100);
    bool        write(const std::string& cmd);
    void        closeWriteChannel();
    void        setEnvVar(const std::string& name, const std::string& val);
    void        addArgument(const std::string& arg);

    Expected<int> interrupt(uint64_t timeoutMs = UNLIMITED );
    Expected<int> kill();

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
    pid_t                    m_pid = 0;
    int                      m_stdout = 0;
    int                      m_stderr = 0;
    int                      m_stdin  = 0;

    std::mutex              m_streamMutex;
    std::stringstream       m_streamOut;
    std::stringstream       m_streamErr;
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

    if(m_stdout) {
        close(m_stdout);
	    m_stdout = 0;
    }

    if(m_stderr) {
        close(m_stderr);
	    m_stderr = 0;
    }

    if (m_pid) {
        interrupt(10000); //wait 10s for th process to finish, if interrupt succeed, the next hard kill will be ignored
        kill();
    }
}

inline Expected<pid_t> Process::run()
{
    int coutPipe[2];
    int cerrPipe[2];
    int cinPipe[2];

    memset(coutPipe, 0, sizeof(coutPipe));
    memset(cerrPipe, 0, sizeof(cerrPipe));
    memset(cinPipe, 0, sizeof(cinPipe));

    try {
        posix_spawn_file_actions_t action;
        posix_spawn_file_actions_init(&action);

        //Create stdout
        if (pipe(coutPipe)) {
            throw std::runtime_error("pipe returned an error");
        }
        posix_spawn_file_actions_addclose(&action, coutPipe[0]);
        posix_spawn_file_actions_adddup2(&action, coutPipe[1], STDOUT_FILENO);


        //Create stderr
        if (pipe(cerrPipe)) {
            throw std::runtime_error("pipe returned an error");
        }
        posix_spawn_file_actions_addclose(&action, cerrPipe[0]);
        posix_spawn_file_actions_adddup2(&action, cerrPipe[1], STDERR_FILENO);

        if (pipe(cinPipe)) {
            throw std::runtime_error("pipe returned an error");
        }
        posix_spawn_file_actions_addclose(&action, cinPipe[1]);
        posix_spawn_file_actions_adddup2(&action, cinPipe[0], STDIN_FILENO);

        CharArray args(m_cmd, m_args);
        CharArray env(m_environ);

        if (posix_spawnp(&m_pid, m_cmd.data(), &action, nullptr, args.data(), env.data()) != 0) {
            throw std::runtime_error("posix_spawnp failed with error: " + std::string(strerror(errno)));
        }

        if (posix_spawn_file_actions_destroy(&action)) {
            throw std::runtime_error("posix_spawn_file_actions_destroy");
        }


        //Setup the stdout file descriptor
        {
            //We close the useless entry
            close(coutPipe[1]);
            m_stdout = coutPipe[0];

            //We set the output as none blocking for the read actions (used in the wait process)
            int o_flags = fcntl(m_stdout, F_GETFL);
            int n_flags = o_flags | O_NONBLOCK;
            fcntl(m_stdout, F_SETFL, n_flags);
        }

        //Setup the stderr file descriptor
        {
            //We close the useless entry
            close(cerrPipe[1]);
            m_stderr = cerrPipe[0];

            //We set the output as none blocking for the read actions (used in the wait process)
            int o_flags = fcntl(m_stderr, F_GETFL);
            int n_flags = o_flags | O_NONBLOCK;
            fcntl(m_stderr, F_SETFL, n_flags);
        }

        //Setup the stdin file descriptor
        {
            //We close the useless entry
            close(cinPipe[0]);
            m_stdin = cinPipe[1];

            //we do not need the stdin, so we close it
            if (!isSet(m_capture, Capture::In)) {
                closeWriteChannel();
            }
        }

        return m_pid;
    }
    catch (const std::exception & e)
    {
        //In case of error, we need to clean all the pipe
        m_stdin = 0;
        if(cinPipe[0]) close(cinPipe[0]);
        if(cinPipe[1]) close(cinPipe[1]);

        m_stdout = 0;
        if(coutPipe[0]) close(coutPipe[0]);
        if(coutPipe[1]) close(coutPipe[1]);

        m_stderr = 0;
        if(cerrPipe[0]) close(cerrPipe[0]);
        if(cerrPipe[1]) close(cerrPipe[1]);

        return unexpected(e.what());
    }
}

//dumpPipeInStream is reading from file descriptor and
// if dumpData is set, it dump the data in the stream, otherwise the data are discarded
inline int dumpPipeInStream(int fd, std::ostream & out, bool dumpData)
{
    if(fd == 0) {
        return -1;
    }

    char buffer[65535];

    int bytesRead = read(fd, buffer, static_cast<size_t>(65535));
    if(bytesRead > 0) {
        if(dumpData) {
            out.write(buffer, bytesRead);
        }
    }

    return bytesRead;
}

inline Expected<int> Process::wait(uint64_t timeoutMs, uint32_t waitCycleDurationMs)
{
    if(!m_pid) {
        return unexpected("Process is not running");
    }

    closeWriteChannel();
    int status = 0;

    // Wait with timeout is hard to do has the OS does not provid direct solution for it.
    // We will do active waiting with step...

    assert(waitCycleDurationMs);
    if (waitCycleDurationMs == 0) {
        return unexpected("Cycle duration has to be bigger than 0");
    }

    // Better to use number of cycle to wait, than using std::chrono, which require a syscall to have exact time.
    uint64_t numberOfCycles = 0;

    // Number of cycle is timeout (ms) / by the cycle duration (ms) and rounded to upper value
    uint64_t maxNumberOfCycles = timeoutMs / waitCycleDurationMs;
    if (timeoutMs % waitCycleDurationMs > 0) {
        maxNumberOfCycles++;
    }

    pid_t pid;
    while (true) // endless loop
    {

        // Check if the process is stopped
        if ((pid = waitpid(m_pid, &status, WNOHANG | WUNTRACED | WCONTINUED)) == -1) {
            return unexpected("waitpid error");
        }

        //Dump the Buffers
        {
            std::lock_guard<std::mutex> guard(m_streamMutex);
            dumpPipeInStream(m_stdout, m_streamOut, isSet(m_capture, Capture::Out));
            dumpPipeInStream(m_stderr, m_streamErr, isSet(m_capture, Capture::Err));
        }

        // Child is still running
        if (pid == 0) {
            // Child is still running

            // Check if we waited enough
            if (numberOfCycles > maxNumberOfCycles) {
                return unexpected("timeout");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(waitCycleDurationMs));
            numberOfCycles++;
        } else {
            //Dump all the buffer => readFromFd should return 0 when it's finished or -1
            int readFromBuffer = 0;
            do {
                std::lock_guard<std::mutex> guard(m_streamMutex);
                readFromBuffer = dumpPipeInStream(m_stdout, m_streamOut, isSet(m_capture, Capture::Out));
            } while (readFromBuffer > 0);

            do {
                std::lock_guard<std::mutex> guard(m_streamMutex);
                readFromBuffer = dumpPipeInStream(m_stderr, m_streamErr, isSet(m_capture, Capture::Err));
            } while (readFromBuffer > 0);

            //set pid to 0 as the process finished
            m_pid = 0;

            // Idenfity why we returned
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {

                std::string errMsg = std::string(strsignal(WTERMSIG(status)));
                if (WCOREDUMP(status)) {
                    errMsg += " (core dumped)";
                }

                return unexpected(errMsg);
            }

            return unexpected("Impossible to identify reason for stop");
        }
    }
}

inline std::string Process::readAllStandardOutput(uint64_t timeoutMs)
{
    std::string str;
    //We do a dump in case there is a bit of data after 100ms (to be backward compatible...)
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

    {
	std::lock_guard<std::mutex> guard(m_streamMutex);
        dumpPipeInStream(m_stdout, m_streamOut, isSet(m_capture, Capture::Out));
        str = m_streamOut.str();

        //clean the buffer
        m_streamOut.clear();
        m_streamOut.str(std::string());
    }

    return str;
}

inline std::string Process::readAllStandardError(uint64_t timeoutMs)
{
    std::string str;

    //We do a dump in case there is a bit of data after 100ms (to be backward compatible...)
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

    {
	std::lock_guard<std::mutex> guard(m_streamMutex);
        dumpPipeInStream(m_stderr, m_streamErr, isSet(m_capture, Capture::Err));
        str = m_streamErr.str();

        //clean the buffer
        m_streamErr.clear();
        m_streamErr.str(std::string());
    }

    return str;
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


inline Expected<int> Process::interrupt(uint64_t timeoutMs)
{
    if (!m_pid) {
        return unexpected("Process is not running");
    }

    ::kill(m_pid, SIGINT);
    return wait(timeoutMs);
}

inline Expected<int> Process::kill()
{
    if (!m_pid) {
        return unexpected("Process is not running");
    }

    ::kill(m_pid, SIGKILL);
    return wait();
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
    auto ret = proc.wait();
    out = proc.readAllStandardOutput();
    err = proc.readAllStandardError();
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
    auto ret = proc.wait();
    out = proc.readAllStandardOutput();
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
