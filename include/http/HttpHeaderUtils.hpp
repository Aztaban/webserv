#ifndef HTTPHEADERUTILS_HPP
# define HTTPHEADERUTILS_HPP

# include <cctype>
# include <map>
# include <string>

namespace HttpHeaderUtils
{
	inline std::string trimOws(const std::string& value)
	{
		std::size_t begin = 0;
		std::size_t end = value.size();

		while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
			begin++;
		while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
			end--;
		return value.substr(begin, end - begin);
	}

	inline std::string lowercase(const std::string& value)
	{
		std::string result = value;

		for (std::size_t i = 0; i < result.size(); i++)
		{
			result[i] = static_cast<char>(
				std::tolower(static_cast<unsigned char>(result[i])));
		}
		return result;
	}

	inline bool getValue(
		const std::map<std::string, std::string>& headers,
		const std::string& name,
		std::string& value)
	{
		const std::string normalizedName = lowercase(name);

		for (std::map<std::string, std::string>::const_iterator it = headers.begin();
			it != headers.end(); ++it)
		{
			if (lowercase(it->first) == normalizedName)
			{
				value = it->second;
				return true;
			}
		}
		return false;
	}
}

#endif
