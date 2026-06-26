#ifndef CLIENTCONNECTION_HPP
# define CLIENTCONNECTION_HPP

# include "core/RequestContext.hpp"
# include "http/HttpResponse.hpp"

# include <string>
# include <ctime>

enum ClientState
{
	CLIENT_READING_HEADERS,
	CLIENT_READING_BODY,
	CLIENT_PROCESSING,
	CLIENT_WRITING,
	CLIENT_WAITING_CGI,
	CLIENT_DONE,
	CLIENT_ERROR
};

/**
 * @brief Manages one connected client socket.
 *
 * ClientConnection owns the client fd, read/write buffers, connection state,
 * and request context. It reads a raw HTTP request, passes the parsed request
 * to RequestHandler, and stores the serialized response until it is sent.
 *
 * It does not own listening sockets, server startup, or the main poll loop.
 */
class ClientConnection {

private:
	static const std::size_t MAX_HEADER_SIZE = 8192;

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
	void		touch();

	/**
	 * @brief Reads available data from the client socket.
	 *
	 * Must only be called after poll() reports the client fd as readable.
	 */
	void		readFromSocket();

	/**
	 * @brief Sends pending response bytes to the client socket.
	 *
	 * Must only be called after poll() reports the client fd as writable.
	 */
	void		writeToSocket();

	/**
	 * @brief Stores a complete HTTP response and switches to writing state.
	 *
	 * This is also the hook used later when a CGI job finishes and the
	 * EventLoop needs to give the final response back to the client.
	 */
	void		prepareResponse(const HttpResponse& response);

	/**
	 * @brief Checks whether the client has been inactive for too long.
	 */
	bool		isTimedOut(std::time_t now, int timeoutSeconds) const;

};

#endif
