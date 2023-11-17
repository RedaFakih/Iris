#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>
#include <map>

namespace vkPlayground {

	class ShaderPreProcessor 
	{
	public:
		ShaderPreProcessor() = default;

		std::map<VkShaderStageFlagBits, std::string> PreprocessShader(const std::string& source) const;

		template<typename InputIt, typename OutputIt>
		void CopyWithoutComments(InputIt first, InputIt last, OutputIt out) const;
		std::vector<std::string> TokenizeString(const std::string& str) const;

	private:
		enum class State : char { SlashOC, StarIC, SingleLineComment, MultiLineComment, NotAComment };
	};

	// From https://wandbox.org/permlink/iXC7DWaU8Tk8jrf3 (modified to remove the '#' comments)
	template<typename InputIt, typename OutputIt>
	void ShaderPreProcessor::CopyWithoutComments(InputIt first, InputIt last, OutputIt out) const
	{
		State state = State::NotAComment;

		while (first != last)
		{
			switch (state)
			{
			case State::SlashOC:
				if (*first == '/') 
					state = State::SingleLineComment;
				else if (*first == '*') 
					state = State::MultiLineComment;
				else
				{
					state = State::NotAComment;
					*out++ = '/';
					*out++ = *first;
				}
				break;
			case State::StarIC:
				if (*first == '/') 
					state = State::NotAComment;
				else 
					state = State::MultiLineComment;
				break;
			case State::NotAComment:
				if (*first == '/') 
					state = State::SlashOC;
				else 
					*out++ = *first;
				break;
			case State::SingleLineComment:
				if (*first == '\n')
				{
					state = State::NotAComment;
					*out++ = '\n';
				}
				break;
			case State::MultiLineComment:
				if (*first == '*') 
					state = State::StarIC;
				else if (*first == '\n') 
					*out++ = '\n';
				break;
			}
			++first;
		}
	}

}