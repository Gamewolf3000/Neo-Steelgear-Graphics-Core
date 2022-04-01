#include "pch.h"

#include "../Neo Steelgear Graphics Core/D3DPtr.h"

class DummyInterface : public IUnknown
{
private:
	ULONG counter = 1;

public:
	DummyInterface() = default;
	~DummyInterface() = default;

	// Inherited via IUnknown
	virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
	{
		return E_NOTIMPL;
	}

	virtual ULONG __stdcall AddRef(void) override
	{
		return ++counter;
	}

	virtual ULONG __stdcall Release(void) override
	{
		return --counter;
	}

	ULONG GetCounter()
	{
		return counter;
	}
};

TEST(D3DPtrTest, DefaultInitialisable)
{
	D3DPtr<DummyInterface> ptr;
	ASSERT_EQ(ptr.Get(), nullptr);
	ASSERT_EQ(ptr.operator->(), nullptr);
	DummyInterface* castedPtr = ptr;
	ASSERT_EQ(castedPtr, nullptr);
}

TEST(D3DPtrTest, ValueInitialisable)
{
	DummyInterface dummy;

	{
		D3DPtr<DummyInterface> ptr = &dummy;
		ASSERT_EQ(ptr.Get(), &dummy);
		ASSERT_EQ(ptr.operator->(), &dummy);
		DummyInterface* castedPtr = ptr;
		ASSERT_EQ(castedPtr, &dummy);
	}

	ASSERT_EQ(dummy.GetCounter(), 0);
}