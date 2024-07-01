#pragma once

#include "StreamReader.h"
#include "StreamWriter.h"

#include <filesystem>
#include <fstream>

namespace Iris {

	class FileStreamWriter : public StreamWriter
	{
	public:
		FileStreamWriter(const std::filesystem::path& filePath, const bool truncate = false);
		FileStreamWriter(const FileStreamWriter&) = delete;
		FileStreamWriter& operator=(const FileStreamWriter&) = delete;
		~FileStreamWriter();

		// Inherited via StreamWriter
		bool IsStreamGood() const override;
		uint64_t GetStreamPosition() override;
		void SetStreamPosition(uint64_t pos) override;
		void WriteData(const uint8_t* data, std::size_t size) override;

	private:
		std::ofstream m_Stream;
	};

	class FileStreamReader : public StreamReader
	{
	public:
		FileStreamReader(const std::filesystem::path& filePath);
		FileStreamReader(const FileStreamReader&) = delete;
		FileStreamReader& operator=(const FileStreamReader&) = delete;
		~FileStreamReader();

		// Inherited via StreamReader
		bool IsStreamGood() const override;
		uint64_t GetStreamPosition() override;
		void SetStreamPosition(uint64_t pos) override;
		void ReadData(uint8_t* dst, std::size_t size) override;

	private:
		std::ifstream m_Stream;
	};
}