#pragma once

#include "Base.h"

namespace vkPlayground {

	// UnSafe Buffer! Remember to always release when no longer needed
	struct Buffer
	{
		uint8_t* Data;
		std::size_t Size;

		constexpr Buffer() noexcept
			: Data(nullptr), Size(0)
		{
		}

		constexpr Buffer(const uint8_t* data, std::size_t size) noexcept
			: Data((uint8_t*)data), Size(size)
		{
		}

		static Buffer Copy(const Buffer& other)
		{
			Buffer result;
			result.Allocate(other.Size);
			std::memcpy(result.Data, other.Data, other.Size);
			return result;
		}

		static Buffer Copy(const uint8_t* data, std::size_t size)
		{
			Buffer result;
			result.Allocate(size);
			std::memcpy(result.Data, data, size);
			return result;
		}

		void Allocate(std::size_t size)
		{
			delete[] Data;
			Data = nullptr;

			if (size == 0)
				return;

			Data = new uint8_t[size];
			Size = size;
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		template<typename T>
		constexpr T& Read(std::size_t offset = 0)
		{
			return *reinterpret_cast<T*>(Data + offset);
		}

		template<typename T>
		constexpr const T& Read(std::size_t offset = 0) const
		{
			return *reinterpret_cast<T*>(Data + offset);
		}

		uint8_t* ReadBytes(std::size_t size, std::size_t offset) const
		{
			PG_ASSERT(size + offset <= Size, "Overflow");
			uint8_t* data = new uint8_t[size];
			std::memcpy(data, Data + offset, size);
			return data;
		}

		void Write(const uint8_t* data, std::size_t size, std::size_t offset = 0)
		{
			PG_ASSERT(size + offset <= Size, "");
			std::memcpy(Data + offset, data, size);
		}

		constexpr operator bool() const noexcept { return Data; }

		uint8_t& operator[](std::size_t index)
		{
			return Data[index];
		}

		uint8_t operator[](std::size_t index) const
		{
			return Data[index];
		}

	};

}