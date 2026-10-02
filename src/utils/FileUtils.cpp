#include "utils/FileUtils.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

namespace
{
	class ScopedFd
	{
		int _fd;
		ScopedFd(const ScopedFd&);
		ScopedFd& operator=(const ScopedFd&);
	public:
		explicit ScopedFd(int fd) : _fd(fd) {}
		~ScopedFd() { if (_fd != -1) close(_fd); }
		int get() const { return _fd; }
		bool finish()
		{
			int fd = _fd;
			_fd = -1;
			return close(fd) == 0;
		}
	};

	bool isRegularFd(int fd)
	{
		std::ostringstream path;
		path << "/dev/fd/" << fd;
		struct stat status;
		return stat(path.str().c_str(), &status) == 0 && S_ISREG(status.st_mode);
	}

	int fileError()
	{
		// Called only after stat/open, never after read/write.
		if (errno == EACCES || errno == EPERM) return 403;
		if (errno == ENOENT || errno == ENOTDIR) return 404;
		return 500;
	}
}

// Checks if file or directory exists
bool FileUtils::exists(const std::string& path)
{
	struct stat s;
	return (stat(path.c_str(), &s) == 0);
}

// Reads a file into a string (binary safe)
std::string FileUtils::read(const std::string& path)
{
	std::string body;
	if (read(path, body) != 0)
		return "";
	return body;
}

int FileUtils::read(const std::string& path, std::string& body)
{
	body.clear();
	struct stat status;
	if (stat(path.c_str(), &status) != 0)
		return fileError();
	if (!S_ISREG(status.st_mode))
		return 403;
	// O_NONBLOCK also protects open if a file is replaced by a FIFO.
	ScopedFd file(open(path.c_str(), O_RDONLY | O_NONBLOCK));
	if (file.get() == -1)
		return fileError();
	if (!isRegularFd(file.get()))
		return 403;
	char buffer[8192];
	ssize_t count;
	while ((count = ::read(file.get(), buffer, sizeof(buffer))) > 0)
		body.append(buffer, count);
	if (count < 0)
	{
		body.clear();
		return 500;
	}
	return 0;
}

bool FileUtils::write(const std::string& path, const std::string& body)
{
	struct stat status;
	if (stat(path.c_str(), &status) == 0 && !S_ISREG(status.st_mode))
		return false;
	ScopedFd file(open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0666));
	if (file.get() == -1 || !isRegularFd(file.get()))
		return false;
	std::size_t offset = 0;
	while (offset < body.size())
	{
		ssize_t count = ::write(file.get(), body.data() + offset, body.size() - offset);
		if (count <= 0)
			return false;
		offset += count;
	}
	return file.finish();
}

// Returns MIME type based on file extension
std::string FileUtils::mimeType(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return "application/octet-stream";

	std::string ext = path.substr(dot);

	if (ext == ".html") return "text/html";
	if (ext == ".txt")  return "text/plain";
	if (ext == ".css")  return "text/css";
	if (ext == ".js")   return "application/javascript";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".png")  return "image/png";

	return "application/octet-stream";
}
