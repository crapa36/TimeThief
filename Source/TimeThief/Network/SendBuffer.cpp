#include "C:\Github\TimeThief\Intermediate\Build\Win64\x64\TimeThiefEditor\Development\UnrealEd\SharedPCH.UnrealEd.Project.ValApi.ValExpApi.Cpp20.h"
#include "SendBuffer.h"

/*--------------
   SendBuffer
--------------*/

SendBuffer::SendBuffer(int32 BufferSize)
{
   buffer_.SetNum(BufferSize);
}

SendBuffer::~SendBuffer()
{
}

void SendBuffer::CopyData(void* data, int32 len)
{
   // THINK: len을 Capacity()보다 크게 보내는 경우에 대한 예외처리 필요?
   ::memcpy(buffer_.GetData(), data, len);
   writeSize_ = len;
}

void SendBuffer::Close(int32 writeSize)
{
   writeSize_ = writeSize;
}
