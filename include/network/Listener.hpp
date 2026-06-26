#ifndef LISTENER_HPP
# define LISTENER_HPP

# include "config/ServerConfig.hpp"

# include <string>
# include <vector>

/**
 * @brief Represents one listening socket.
 *
 * Listener owns a socket bound to one host:port pair. It accepts new client
 * connections and stores the ServerConfig candidates that belong to this
 * listener.
 *
 * It does not read from or write to connected client sockets.
 */
class Listener {

private:
	std::string	_host;
	int			_port;
	int			_fd;
	std::vector<ServerConfig>	_serverCandidates;

	Listener(const Listener &other);
	Listener	&operator=(const Listener &other);
	void		_setNonBlocking(int fd);

public:
	Listener(const std::string& host, int port);
	Listener(const std::string& host, int port, const std::vector<ServerConfig>& serverCandidates);
	~Listener();

	/**
	 * @brief Creates, binds, and starts the listening socket.
	 */
	void	setup();

	int		getFd() const;
	int		getPort() const;
	const std::string&	getHost() const;
	const std::vector<ServerConfig>&	getServerCandidates() const;

	/**
	 * @brief Accepts one pending client connection.
	 *
	 * Must only be called after poll() reports the listener fd as readable.
	 * Returns -1 when no client can be accepted.
	 */
	int		acceptClient() const;
};

#endif
