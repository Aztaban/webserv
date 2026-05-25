#ifndef CLIENTCONNECTION_HPP
# define CLIENTCONNECTION_HPP

# include <string>
# include <ctime>

enum ClientState
{
    CLIENT_READING_HEADERS,
    CLIENT_READING_BODY,
    CLIENT_PROCESSING,
    CLIENT_WRITING,
    CLIENT_DONE,
    CLIEANT_ERROR
};

class ClientConnection {

private:
    static const std::size_t MAX_HEADER_SIZE = 8192;

    int         _fd;
    ClientState _state;
    std::string _readBuffer;
    std::string _writeBuffer;
    std::time_t _lastActivity;

    ClientConnection(const ClientConnection &other);
    ClientConnection &operator=(const ClientConnection &other);
    void    prepareHardcodeResponse();
    void    prepareErrorResponse(int code);

public:
    ClientConnection(int fd);
    ~ClientConnection();

    int         getFd() const;
    ClientState getState() const;
    bool        wantsRead() const;
    bool        wantsWrite() const;
    bool        isDone() const;
    void        readFromSocket();
    void        writeToSocket();
    void        touch();
    bool        isTimedOut(std::time_t now, int timeoutSeconds) const;

};

#endif