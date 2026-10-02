#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

class FileUtils
{
	public:
		static bool exists(const std::string& path);
		static std::string read(const std::string& path);
		// Returns zero on success, or an HTTP error status; empty files succeed.
		static int read(const std::string& path, std::string& body);
		static bool write(const std::string& path, const std::string& body);
		static std::string mimeType(const std::string& path);

	private:
		FileUtils(); // prevent instantiation
};

#endif