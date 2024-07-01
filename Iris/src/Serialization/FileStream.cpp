#include "IrisPCH.h"
#include "FileStream.h"

namespace Iris {

    FileStreamWriter::FileStreamWriter(const std::filesystem::path& filePath, const bool truncate)
        : m_Stream(filePath, std::ios::out | std::ios::binary | (truncate ? std::ios::trunc : 0))
    {
    }

    FileStreamWriter::~FileStreamWriter()
    {
        m_Stream.close();
    }

    bool FileStreamWriter::IsStreamGood() const
    {
        return m_Stream.good();
    }

    uint64_t FileStreamWriter::GetStreamPosition()
    {
        return m_Stream.tellp();
    }

    void FileStreamWriter::SetStreamPosition(uint64_t pos)
    {
        m_Stream.seekp(pos);
    }

    void FileStreamWriter::WriteData(const uint8_t* data, std::size_t size)
    {
        m_Stream.write(reinterpret_cast<const char*>(data), size);
    }

    FileStreamReader::FileStreamReader(const std::filesystem::path& filePath)
        : m_Stream(filePath, std::ios::in | std::ios::binary)
    {
    }

    FileStreamReader::~FileStreamReader()
    {
        m_Stream.close();
    }

    bool FileStreamReader::IsStreamGood() const
    {
        return m_Stream.good();
    }

    uint64_t FileStreamReader::GetStreamPosition()
    {
        return m_Stream.tellg();
    }

    void FileStreamReader::SetStreamPosition(uint64_t pos)
    {
        m_Stream.seekg(pos);
    }

    void FileStreamReader::ReadData(uint8_t* dst, std::size_t size)
    {
        m_Stream.read(reinterpret_cast<char*>(dst), size);
    }

}

