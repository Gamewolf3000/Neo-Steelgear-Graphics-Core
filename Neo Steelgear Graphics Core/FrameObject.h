#pragma once

#include <array>
#include <type_traits>
#include <functional>

#include "FrameBased.h"

template<typename T, FrameType Frames>
class FrameObject : public FrameBased<Frames>
{
protected:
	std::array<T, Frames> frameObjects;

public:
	FrameObject() = default;
	virtual ~FrameObject() = default;

	FrameObject(const FrameObject& other) = delete;
	FrameObject& operator=(const FrameObject& other) = delete;
	FrameObject(FrameObject&& other) = default;
	FrameObject& operator=(FrameObject&& other) = default;

	T& Active();
	const T& Active() const;
	T& Next();
	const T& Next() const;
	T& Last();
	const T& Last() const;

	void Initialize(const std::function<void(FrameType, T&)> initFunc);
	void Initialize(const std::function<void(T&)> initFunc);

	template<typename U = T, typename... InitializationTypes,
		typename = std::enable_if_t<std::is_class<U>::value>>
		void Initialize(void(U::* func)(InitializationTypes...),
			InitializationTypes... initializationArgument)
	{
		for (auto& object : frameObjects)
			(object.*(func))(initializationArgument...);
	}
};

template<typename T, std::uint8_t Frames>
inline T& FrameObject<T, Frames>::Active()
{
	return frameObjects[this->activeFrame];
}

template<typename T, std::uint8_t Frames>
inline const T& FrameObject<T, Frames>::Active() const
{
	return frameObjects[this->activeFrame];
}

template<typename T, std::uint8_t Frames>
inline T& FrameObject<T, Frames>::Next()
{
	std::uint8_t nextFrame = (this->activeFrame + 1 == Frames) ? 0 :
		this->activeFrame + 1;
	return frameObjects[nextFrame];
}

template<typename T, std::uint8_t Frames>
inline const T& FrameObject<T, Frames>::Next() const
{
	std::uint8_t nextFrame = (this->activeFrame + 1 == Frames) ? 0 :
		this->activeFrame + 1;
	return frameObjects[nextFrame];
}

template<typename T, std::uint8_t Frames>
inline T& FrameObject<T, Frames>::Last()
{
	std::uint8_t lastFrame = (this->activeFrame == 0) ? Frames - 1 :
		this->activeFrame - 1;
	return frameObjects[lastFrame];
}

template<typename T, std::uint8_t Frames>
inline const T& FrameObject<T, Frames>::Last() const
{
	std::uint8_t lastFrame = (this->activeFrame == 0) ? Frames - 1 :
		this->activeFrame - 1;
	return frameObjects[lastFrame];
}

template<typename T, FrameType Frames>
inline void FrameObject<T, Frames>::Initialize(
	const std::function<void(FrameType, T&)> initFunc)
{
	for (FrameType i = 0; i < Frames; ++i)
		initFunc(i, frameObjects[i]);
}

template<typename T, FrameType Frames>
inline void FrameObject<T, Frames>::Initialize(
	const std::function<void(T&)> initFunc)
{
	for (FrameType i = 0; i < Frames; ++i)
		initFunc(frameObjects[i]);
}