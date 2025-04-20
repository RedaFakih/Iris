#pragma once

#include <filesystem>

namespace Iris::Utils {

	std::string ToLower(const std::string_view string);
	std::string ToUpper(const std::string_view string);
	std::vector<std::string> SplitString(const std::string_view string, const std::string_view delimiter);
	std::vector<uint32_t> SplitStringToUint(const std::string_view string, const std::string_view delimiter);

	std::string RemoveExtension(const std::string& filename);

	std::string BytesToString(uint64_t bytes);

	std::string TrimWhitespace(const std::string& str);

}