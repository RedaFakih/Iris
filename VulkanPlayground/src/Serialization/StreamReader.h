#pragma once

#include <map>
#include <string>
#include <unordered_map>

namespace vkPlayground {

	class StreamReader
	{
	public:
		virtual ~StreamReader() = default;

		virtual bool IsStreamGood() const = 0;
		virtual uint64_t GetStreamPosition() = 0;
		virtual void SetStreamPosition(uint64_t pos) = 0;
		virtual void ReadData(uint8_t* dst, std::size_t size) = 0;

		operator bool() const { return IsStreamGood(); }

		void ReadString(std::string& str)
		{
			std::size_t size;
			ReadData(reinterpret_cast<uint8_t*>(&size), sizeof(std::size_t));
			str.resize(size);
			ReadData(reinterpret_cast<uint8_t*>(str.data()), sizeof(char) * size);
		}

		template<typename T>
		void ReadRaw(T& output)
		{
			ReadData(reinterpret_cast<uint8_t*>(&output), sizeof(T));
		}

		template<typename T>
		void ReadObject(T& obj)
		{
			T::Deserialize(this, obj);
		}

		template<typename Key, typename Val>
		void ReadMap(std::map<Key, Val>& map, uint32_t size = 0)
		{
			if (size == 0)
				ReadRaw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++)
			{
				Key key;
				if constexpr (std::is_trivial_v<Key>)
					ReadRaw<Key>(key);
				else
					ReadObject<Key>(key);

				if constexpr (std::is_trivial_v<Val>)
					ReadRaw<Val>(map[key]);
				else
					ReadObject<Val>(map[key]);
			}
		}

		template<typename Val>
		void ReadMap(std::map<std::string, Val>& map, uint32_t size = 0)
		{
			if (size == 0)
				ReadRaw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++)
			{
				std::string key;
				ReadString(key);

				if constexpr (std::is_trivial_v<Val>)
					ReadRaw<Val>(map[key]);
				else
					ReadObject<Val>(map[key]);
			}
		}

		template<typename Key, typename Val>
		void ReadMap(std::unordered_map<Key, Val>& map, uint32_t size = 0)
		{
			if (size == 0)
				ReadRaw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++)
			{
				Key key;
				if constexpr (std::is_trivial_v<Key>)
					ReadRaw<Key>(key);
				else
					ReadObject<Key>(key);

				if constexpr (std::is_trivial_v<Val>)
					ReadRaw<Val>(map[key]);
				else
					ReadObject<Val>(map[key]);
			}
		}

		template<typename Val>
		void ReadMap(std::unordered_map<std::string, Val>& map, uint32_t size = 0)
		{
			if (size == 0)
				ReadRaw<uint32_t>(size);

			for (uint32_t i = 0; i < size; i++)
			{
				std::string key;
				ReadString(key);

				if constexpr (std::is_trivial_v<Val>)
					ReadRaw<Val>(map[key]);
				else
					ReadObject<Val>(map[key]);
			}
		}

		template<typename T>
		void ReadArray(std::vector<T>& array, uint32_t size = 0)
		{
			if (size == 0)
				ReadRaw<uint32_t>(size);

			array.resize(size);

			for (uint32_t i = 0; i < size; i++)
			{
				if constexpr (std::is_trivial_v<T>)
					ReadRaw<T>(array[i]);
				else
					ReadObject<T>(array[i]);
			}
		}

		template<>
		void ReadArray(std::vector<std::string>& array, uint32_t size)
		{
			if (size == 0)
				ReadRaw<uint32_t>(size);

			array.resize(size);

			for (uint32_t i = 0; i < size; i++)
				ReadString(array[i]);
		}
	};

}