#pragma once

#include <stdint.h>

typedef std::uint8_t FrameType;
template<FrameType Frames>
class FrameBased
{
private:

protected:
	FrameType activeFrame = Frames - 1;

public:
	FrameBased() = default;
	virtual ~FrameBased() = default;

	FrameBased(const FrameBased& other) = delete;
	FrameBased& operator=(const FrameBased& other) = delete;
	FrameBased(FrameBased&& other) = default;
	FrameBased& operator=(FrameBased&& other) = default;

	virtual void SwapFrame();

	FrameType PeekNext() const;
};

template<FrameType Frames>
inline void FrameBased<Frames>::SwapFrame()
{
	activeFrame = (activeFrame + 1 == Frames) ? 0 : activeFrame + 1;
}

template<FrameType Frames>
inline FrameType FrameBased<Frames>::PeekNext() const
{
	return (activeFrame + 1 == Frames) ? 0 : activeFrame + 1;
}
