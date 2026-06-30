#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

class FileUtils
{
	public:
		static bool exists(const std::string& path);
		static std::string read(const std::string& path);
		static std::string mimeType(const std::string& path);

	private:
		FileUtils(); // prevent instantiation
};

#endif