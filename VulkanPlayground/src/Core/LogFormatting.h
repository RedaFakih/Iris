#pragma once

#include <spdlog/fmt/fmt.h>
#include <glm/glm.hpp>

namespace fmt {

	template<>
	struct formatter<glm::vec2>
	{
		char presentation = 'f';

		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
		{
			const char* start = ctx.begin();
			const char* end = ctx.end();

			while (start != end && (*start == 'f' || *start == 'e'))
				presentation = *start++;

			if (start != end && *start != '}')
				throw format_error("Invalid format!");

			return start;
		}

		template<typename FormatContext>
		auto format(const glm::vec2& vec, FormatContext& ctx) const -> decltype(ctx.out())
		{
			return presentation == 'f'
				? fmt::format_to(ctx.out(), "[{:.3f}, {:.3f}]", vec.x, vec.y)
				: fmt::format_to(ctx.out(), "[{:.3e}, {:.3e}]", vec.x, vec.y);
		}
	};

	template<>
	struct formatter<glm::vec3>
	{
		char presentation = 'f';

		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
		{
			const char* start = ctx.begin();
			const char* end = ctx.end();

			while (start != end && (*start == 'f' || *start == 'e'))
				presentation = *start++;

			if (start != end && *start != '}')
				throw format_error("Invalid format!");

			return start;
		}

		template<typename FormatContext>
		auto format(const glm::vec3& vec, FormatContext& ctx) const -> decltype(ctx.out())
		{
			return presentation == 'f'
				? fmt::format_to(ctx.out(), "[{:.3f}, {:.3f}, {:.3f}]", vec.x, vec.y, vec.z)
				: fmt::format_to(ctx.out(), "[{:.3e}, {:.3e}, {:.3e}]", vec.x, vec.y, vec.z);
		}
	};

	template<>
	struct formatter<glm::vec4>
	{
		char presentation = 'f';

		constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin())
		{
			const char* start = ctx.begin();
			const char* end = ctx.end();

			while (start != end && (*start == 'f' || *start == 'e'))
				presentation = *start++;

			if (start != end && *start != '}')
				throw format_error("Invalid format!");

			return start;
		}

		template<typename FormatContext>
		auto format(const glm::vec4& vec, FormatContext& ctx) const -> decltype(ctx.out())
		{
			return presentation == 'f'
				? fmt::format_to(ctx.out(), "[{:.3f}, {:.3f}, {:.3f}, {:.3f}]", vec.x, vec.y, vec.z, vec.w)
				: fmt::format_to(ctx.out(), "[{:.3e}, {:.3e}, {:.3e}, {:.3e}]", vec.x, vec.y, vec.z, vec.w);
		}
	};

}