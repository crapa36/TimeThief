#pragma once

#include "CoreMinimal.h"

/*--------------
   SendBuffer
--------------*/
//
// SendBuffer는 Client에서 사용하는 송신용 버퍼 클래스입니다.
//

class SendBuffer : public TSharedFromThis<SendBuffer>
{
public:
	SendBuffer(int32 BufferSize);
	~SendBuffer();

	BYTE* Buffer() { return buffer_.GetData(); }
	int32 WriteSize() { return writeSize_; }
	int32 Capacity() { return static_cast<int32>(buffer_.Num()); }

	void CopyData(void* data, int32 len);
	void Close(int32 writeSize);
	
private:
	TArray<BYTE> buffer_;
	int32 writeSize_;
	
};
