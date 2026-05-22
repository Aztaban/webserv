#ifndef CLIENTCONNECTION_HPP
# define CLIENTCONNECTION_HPP

#include <string>

enum ClientState
{
    CLIENT_READING,
    CLIENT_WRITING,
    CLIENT_DONE
};

class ClientConnection {

private:
    int         _fd;
    ClientState _state;
    std::string _readBuffer;
    std::string _writeBuffer;

    ClientConnection(const ClientConnection &other);
    ClientConnection &operator=(const ClientConnection &other);
    void    prepareHardcodeResponse();

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

};

#endif