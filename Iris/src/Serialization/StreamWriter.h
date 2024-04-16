#pragma once

#include <map>
#include <string>
#include <unordered_map>

namespace Iris {

	class StreamWriter
	{
	public:
		virtual ~StreamWriter() = default;

		virtual bool IsStreamGood() const = 0;
		virtual uint64_t GetStreamPosition() = 0;
		virtual void SetStreamPosition(uint64_t pos) = 0;
		virtual void WriteData(const uint8_t* data, std::size_t size) = 0;

		operator bool() const { return IsStreamGood(); }

		void WriteZero(std::size_t size)
		{
			uint8_t data = 0;
			for (std::size_t i = 0; i < size; i++)
				WriteData(&data, 1);
		}

		void WriteString(const std::string& str)
		{
			std::size_t size = str.size();
			WriteData(reinterpret_cast<uint8_t*>(&size), sizeof(std::size_t));
			WriteData(reinterpret_cast<const uint8_t*>(str.data()), sizeof(char) * size);
		}

		template<typename T>
		void WriteRaw(const T& type)
		{
			WriteData(reinterpret_cast<const uint8_t*>(&type), sizeof(T));
		}

		template<typename T>
		void WriteObject(const T& obj)
		{
			T::Serialize(this, obj);
		}

		template<typename Key, typename Val>
		void WriteMap(const std::map<Key, Val>& map, bool writeSize = true)
		{
			if (writeSize)
				WriteRaw<uint32_t>(static_cast<uint32_t>(map.size()));

			for (const auto& [key, val] : map)
			{
				if constexpr (std::is_trivial_v<Key>)
					WriteRaw<Key>(key);
				else
					WriteObject<Key>(key);

				if constexpr (std::is_trivial_v<Val>)
					WriteRaw<Val>(val);
				else
					WriteObject<Val>(val);
			}
		}

		template<typename Val>
		void WriteMap(const std::map<std::string, Val>& map, bool writeSize = true)
		{
			if (writeSize)
				WriteRaw<uint32_t>(static_cast<uint32_t>(map.size()));

			for (const auto& [key, value] : map)
			{
				WriteString(key);

				if constexpr (std::is_trivial_v<Val>)
					WriteRaw<Val>(value);
				else
					WriteObject<Val>(value);
			}
		}

		template<typename Key, typename Val>
		void WriteMap(const std::unordered_map<Key, Val>& map, bool writeSize = true)
		{
			if (writeSize)
				WriteRaw<uint32_t>(static_cast<uint32_t>(map.size()));

			for (const auto& [key, val] : map)
			{
				if constexpr (std::is_trivial_v<Key>)
					WriteRaw<Key>(key);
				else
					WriteObject<Key>(key);

				if constexpr (std::is_trivial_v<Val>)
					WriteRaw<Val>(val);
				else
					WriteObject<Val>(val);
			}
		}

		template<typename Val>
		void WriteMap(const std::unordered_map<std::string, Val>& map, bool writeSize = true)
		{
			if (writeSize)
				WriteRaw<uint32_t>(static_cast<uint32_t>(map.size()));

			for (const auto& [key, value] : map)
			{
				WriteString(key);

				if constexpr (std::is_trivial_v<Val>)
					WriteRaw<Val>(value);
				else
					WriteObject<Val>(value);
			}
		}

		template<typename T>
		void WriteArray(const std::vector<T>& array, bool writeSize = true)
		{
			if (writeSize)
				WriteRaw<uint32_t>(static_cast<uint32_t>(array.size()));

			for (const auto& element : array)
			{
				if constexpr (std::is_trivial_v<T>)
					WriteRaw<T>(element);
				else
					WriteObject<T>(element);
			}
		}

		template<>
		void WriteArray(const std::vector<std::string>& array, bool writeSize)
		{
			if (writeSize)
				WriteRaw<uint32_t>(static_cast<uint32_t>(array.size()));

			for (const auto& element : array)
				WriteString(element);
		}
	};

}