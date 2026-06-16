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
	CLIENT_ERROR
};

class ClientConnection {

private:
	static const std::size_t MAX_HEADER_SIZE = 8192; // TEMP: later probably move header/body limits to config/parser
	static const std::size_t MAX_BODY_SIZE = 1000000;

	int			_fd;
	ClientState	_state;
	std::string	_readBuffer; // Network input buffer; later feed this data into HttpRequestParser
	std::string	_writeBuffer; // Serialized response buffer; later filled by HttpResponseBuilder
	std::time_t	_lastActivity;

	// TEMP: used only until real HttpRequestParser handles body length
	std::size_t	_headerEnd;
	std::size_t	_contentLength;

	ClientConnection(const ClientConnection &other);
	ClientConnection &operator=(const ClientConnection &other);

	bool		headersComplete() const; // TEMP: remove when HttpRequestParser is plugged in
	void		processReadBuffer();  // TEMP: replace with HttpRequestParser state handling
	void		parseTempContentLength(); // TEMP: replace with parser header parsing
	std::size_t	getBodyBytesReceived() const; // TEMP: parser/request object should know this later
	void		prepareErrorResponse(int code); // TEMP: later move to ErrorResponseBuilder / HttpResponseBuilder

public:
	ClientConnection(int fd);
	~ClientConnection();

	int			getFd() const;
	ClientState	getState() const;
	bool		wantsRead() const;
	bool		wantsWrite() const;
	bool		isDone() const;
	void		readFromSocket();
	void		writeToSocket();
	void		touch();
	bool		isTimedOut(std::time_t now, int timeoutSeconds) const;

};

#endif