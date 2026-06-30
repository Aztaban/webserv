#include "utils/FileUtils.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// Checks if file or directory exists
bool FileUtils::exists(const std::string& path)
{
	struct stat s;
	return (stat(path.c_str(), &s) == 0);
}

// Reads a file into a string (binary safe)
std::string FileUtils::read(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
		return "";

	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
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
