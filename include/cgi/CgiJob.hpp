#ifndef CGIJOB_HPP
# define CGIJOB_HPP

# include "cgi/CgiRequest.hpp"
# include "http/HttpResponse.hpp"

# include <string>
# include <ctime>

# include <sys/types.h>

class CgiJob {

private:
    pid_t       _pid;
    int         _stdinFd;   // Both fds must be non-blocking and controlled by poll()
    int         _stdoutFd;  // The EventLoop should own CGI jobs because it owns the poll() set.

    std::string _inputBuffer;
    std::string _inputOffset;
    std::string _outputBuffer;

    bool        _stdinClosed;
    bool        _stdoutClosed;
    bool        _done;
    bool        _failed;

    std::time_t _startTime;

public:
    CgiJob();
    ~CgiJob();

    void    start(const CgiRequest& request);

    int     getStdinFd() const;
    int     getStdoutFd() const;
    pid_t   getPid() const;

    bool    wantsWrite() const;
    bool    wantsRead() const;
    bool    isDone() const;
    bool    hasFailed() const;
    bool    isTimedOut(std::time_t now, int timeoutSeconds) const;

    void    writeToCgi();
    void    readFromCgi();

    void    closeStdin();
    void    closeStdout();
    void    killProcess();

    HttpResponse    buildResponse() const;

};

#endif