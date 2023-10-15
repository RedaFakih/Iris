#pragma once

#include <filesystem>

namespace vkPlayground::Utils {

	std::string ToLower(const std::string_view string);
	std::string ToUpper(const std::string_view string);
	std::vector<std::string> SplitString(const std::string_view string, const std::string_view delimiter);

	std::string BytesToString(uint64_t bytes);

}