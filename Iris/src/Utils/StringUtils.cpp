#include "IrisPCH.h"
#include "StringUtils.h"

namespace Iris::Utils {

	std::string ToLower(const std::string_view string)
	{
		std::string retVal;
		retVal.resize(string.size());

		for (std::size_t i = 0; i < string.size(); i++)
			retVal[i] = static_cast<char>(std::tolower(string[i]));

		return retVal;
	}

	std::string ToUpper(const std::string_view string)
	{
		std::string retVal;
		retVal.resize(string.size());

		for (std::size_t i = 0; i < string.size(); i++)
			retVal[i] = static_cast<char>(std::toupper(string[i]));

		return retVal;
	}

	std::vector<std::string> SplitString(const std::string_view string, const std::string_view delimiter)
	{
		std::size_t first = 0;
		std::vector<std::string> retVal;

		while (first <= string.size())
		{
			std::size_t second = string.find_first_of(delimiter, first);

			if (first != second)
				retVal.emplace_back(string.substr(first, second - first));

			if (second == std::string::npos)
				break;

			first = second + 1;
		}

		return retVal;
	}

	std::string RemoveExtension(const std::string& filename)
	{
		return filename.substr(0, filename.find_last_of('.'));
	}

	std::string BytesToString(uint64_t bytes)
	{
		constexpr uint64_t GB = 1024 * 1024 * 1024;
		constexpr uint64_t MB = 1024 * 1024;
		constexpr uint64_t KB = 1024;

		char buffer[33]{};

		if (bytes > GB)
			snprintf(buffer, 32, "%.2f GB", (float)bytes / (float)GB);
		else if (bytes > MB)
			snprintf(buffer, 32, "%.2f MB", (float)bytes / (float)MB);
		else if (bytes > KB)
			snprintf(buffer, 32, "%.2f KB", (float)bytes / (float)KB);
		else
			snprintf(buffer, 32, "%.2f Bytes", (float)bytes);

		return buffer;
	}



}