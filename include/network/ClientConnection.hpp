#ifndef CLIENTCONNECTION_HPP
# define CLIENTCONNECTION_HPP

# include "core/RequestContext.hpp"
#include "http/HttpResponse.hpp"

# include <string>
# include <ctime>

enum ClientState
{
	CLIENT_READING_HEADERS,
	CLIENT_READING_BODY,
	CLIENT_PROCESSING,
	CLIENT_WRITING,
	CLIENT_DONE,
	CLIENT_ERROR
};

class ClientConnection {

private:
	static const std::size_t MAX_HEADER_SIZE = 8192; // TEMP: later probably move header/body limits to config/parser

	int				_fd;
	ClientState		_state;
	std::string		_readBuffer;
	std::string		_writeBuffer;
	std::time_t		_lastActivity;
	RequestContext	_context;

	ClientConnection(const ClientConnection &other);
	ClientConnection &operator=(const ClientConnection &other);

	void		_processReadBuffer();
	void		_prepareErrorResponse(int code);
	std::size_t	_getClientMaxBodySize() const;
	bool		_bodyExceedsLimit() const;

public:
	ClientConnection(int fd, const RequestContext& context);
	~ClientConnection();

	int			getFd() const;
	ClientState	getState() const;
	bool		wantsRead() const;
	bool		wantsWrite() const;
	bool		isDone() const;
	void		readFromSocket();
	void		writeToSocket();

	void		prepareResponse(const HttpResponse& response);
	void		touch();
	bool		isTimedOut(std::time_t now, int timeoutSeconds) const;

};

#endif
