#pragma once

#include "Core/Buffer.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

namespace Iris {

	class JoltBinaryStreamReader : public JPH::StreamIn
	{
	public:
		JoltBinaryStreamReader(const Buffer& buffer)
			: m_Buffer(&buffer)
		{
		}

		~JoltBinaryStreamReader()
		{
			m_Buffer = nullptr;
			m_ReadBytes = 0;
		}

		virtual void ReadBytes(void* outData, size_t inNumBytes) override
		{
			std::memcpy(outData, static_cast<void*>(static_cast<uint8_t*>(m_Buffer->Data) + m_ReadBytes), inNumBytes);
			m_ReadBytes += inNumBytes;
		}

		virtual bool IsEOF() const override { return m_Buffer == nullptr || m_ReadBytes > m_Buffer->Size; }
		
		virtual bool IsFailed() const override { return m_Buffer == nullptr || m_Buffer->Data == nullptr || m_Buffer->Size == 0; }

	private:
		const Buffer* m_Buffer = nullptr;
		size_t m_ReadBytes = 0;

	};

	class JoltBinaryStreamWriter : public JPH::StreamOut
	{
	public:
		void WriteBytes(const void* inData, size_t inNumBytes) override
		{
			size_t currentOffset = m_TempBuffer.size();
			m_TempBuffer.resize(currentOffset + inNumBytes);
			std::memcpy(m_TempBuffer.data() + currentOffset, inData, inNumBytes);
		}

		virtual bool IsFailed() const override { return false; }
		
		Buffer ToBuffer() const
		{
			return Buffer::Copy(m_TempBuffer.data(), static_cast<uint32_t>(m_TempBuffer.size()));
		}

	private:
		std::vector<uint8_t> m_TempBuffer;

	};

}