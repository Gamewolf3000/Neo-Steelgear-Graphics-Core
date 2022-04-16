#include "pch.h"

#include <string>
#include <array>

#include "../Neo Steelgear Graphics Core/HeapHelper.h"

TEST(HeapHelperTest, DefaultInitialisable)
{
	HeapHelper<int> intHeapHelper;
	HeapHelper<float> floatHeapHelper;
	HeapHelper<std::string> stringHeapHelper;
}

template<typename T>
void TestRuntimeInitialisation(size_t heapSize)
{
	HeapHelper<T> helper;
	helper.Initialize(heapSize);
	ASSERT_EQ(helper.TotalSize(), heapSize);
	ASSERT_EQ(helper.GetStartOfChunk(0), 0);
}

template<typename T>
void TestRuntimeInitialisation(size_t heapSize, const T& specific)
{
	HeapHelper<T> helper;
	helper.Initialize(heapSize, specific);
	ASSERT_EQ(helper.TotalSize(), heapSize);
	ASSERT_EQ(helper.GetStartOfChunk(0), 0);
	ASSERT_EQ(helper[0], specific);
}

TEST(HeapHelperTest, RuntimeInitialisable)
{
	TestRuntimeInitialisation<int>(100);
	TestRuntimeInitialisation<float>(1000);
	TestRuntimeInitialisation<std::string>(10000);

	TestRuntimeInitialisation<int>(1000, 2);
	TestRuntimeInitialisation<float>(10000, 2.0f);
	TestRuntimeInitialisation<std::string>(100000, std::to_string(2));
}

template<typename T>
void TestAllocation(HeapHelper<T>& helper, size_t allocationSize,
	AllocationStrategy strategy, size_t alignment,
	size_t expectedChunkIndex, size_t expectedChunkStart)
{
	size_t totalSizeBefore = helper.TotalSize();
	size_t index = helper.AllocateChunk(allocationSize, strategy, alignment);
	ASSERT_EQ(index, expectedChunkIndex);
	ASSERT_EQ(totalSizeBefore, helper.TotalSize());

	if(index != size_t(-1))
		ASSERT_EQ(expectedChunkStart, helper.GetStartOfChunk(index));
}

TEST(HeapHelperTest, HandlesSimpleAllocations)
{
	std::array<AllocationStrategy, 3> strategies = {
		AllocationStrategy::FIRST_FIT, AllocationStrategy::BEST_FIT,
		AllocationStrategy::WORST_FIT };
	for (auto& strategy : strategies)
	{
		HeapHelper<int> intHelper;
		HeapHelper<float> floatHelper;
		HeapHelper<std::string> stringHelper;
		intHelper.Initialize(1000 * sizeof(int));
		floatHelper.Initialize(1000 * sizeof(float));
		stringHelper.Initialize(1000 * sizeof(std::string));

		for (size_t i = 0; i < 1000; ++i)
		{
			TestAllocation(intHelper, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelper, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelper, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));
		}

		TestAllocation(intHelper, sizeof(int), strategy,
			alignof(int), size_t(-1), 0);
		TestAllocation(floatHelper, sizeof(float), strategy,
			alignof(float), size_t(-1), 0);
		TestAllocation(stringHelper, sizeof(std::string), strategy,
			alignof(std::string), size_t(-1), 0);
	}
}

TEST(HeapHelperTest, HandlesIntermediateAllocations)
{
	std::array<AllocationStrategy, 3> strategies = {
		AllocationStrategy::FIRST_FIT, AllocationStrategy::BEST_FIT,
		AllocationStrategy::WORST_FIT };
	for (auto& strategy : strategies)
	{
		HeapHelper<int> intHelper;
		HeapHelper<float> floatHelper;
		HeapHelper<std::string> stringHelper;
		intHelper.Initialize(1000 * sizeof(int));
		floatHelper.Initialize(1000 * sizeof(float));
		stringHelper.Initialize(1000 * sizeof(std::string));
		size_t expectedOffset = 0;

		for (size_t i = 0; i < 20; ++i)
		{
			TestAllocation(intHelper, i * sizeof(int), strategy,
				alignof(int), i, expectedOffset * sizeof(int));
			TestAllocation(floatHelper, i * sizeof(float), strategy,
				alignof(float), i, expectedOffset * sizeof(float));
			TestAllocation(stringHelper, i * sizeof(std::string), strategy,
				alignof(std::string), i, expectedOffset * sizeof(std::string));

			expectedOffset += i;
		}

		TestAllocation(intHelper, sizeof(int) * 1000, strategy,
			alignof(int), size_t(-1), 0);
		TestAllocation(floatHelper, sizeof(float) * 1000, strategy,
			alignof(float), size_t(-1), 0);
		TestAllocation(stringHelper, sizeof(std::string) * 1000, strategy,
			alignof(std::string), size_t(-1), 0);
	}
}

TEST(HeapHelperTest, HandlesAdvancedAllocations)
{
	AllocationStrategy strategy = AllocationStrategy::BEST_FIT;
	HeapHelper<int> intHelper;
	HeapHelper<float> floatHelper;
	HeapHelper<std::string> stringHelper;
	intHelper.Initialize(1000);
	floatHelper.Initialize(1000);
	stringHelper.Initialize(1000);
	size_t expectedOffset = 0;

	TestAllocation(intHelper, 33, strategy, 1, 0, 0);
	TestAllocation(floatHelper, 33, strategy, 1, 0, 0);
	TestAllocation(stringHelper, 33, strategy, 1, 0, 0);

	TestAllocation(intHelper, 5, strategy, 1, 1, 33);
	TestAllocation(floatHelper, 5, strategy, 1, 1, 33);
	TestAllocation(stringHelper, 5, strategy, 1, 1, 33);

	TestAllocation(intHelper, 128, strategy, 256, 2, 256);
	TestAllocation(floatHelper, 128, strategy, 256, 2, 256);
	TestAllocation(stringHelper, 128, strategy, 256, 2, 256);

	TestAllocation(intHelper, 16, strategy, 64, 3, 64);
	TestAllocation(floatHelper, 16, strategy, 64, 3, 64);
	TestAllocation(stringHelper, 16, strategy, 64, 3, 64);

	TestAllocation(intHelper, 32, strategy, 256, 4, 512);
	TestAllocation(floatHelper, 32, strategy, 256, 4, 512);
	TestAllocation(stringHelper, 32, strategy, 256, 4, 512);

	TestAllocation(intHelper, 144, strategy, 32, 6, 96);
	TestAllocation(floatHelper, 144, strategy, 32, 6, 96);
	TestAllocation(stringHelper, 144, strategy, 32, 6, 96);
}

template<typename T>
void TestDeallocation(HeapHelper<T>& helper, size_t indexToRemove)
{
	size_t totalSizeBefore = helper.TotalSize();
	helper.DeallocateChunk(indexToRemove);
	ASSERT_EQ(totalSizeBefore, helper.TotalSize());
}

TEST(HeapHelperTest, HandlesDeallocations)
{
	std::array<AllocationStrategy, 3> strategies = {
		AllocationStrategy::FIRST_FIT, AllocationStrategy::BEST_FIT,
		AllocationStrategy::WORST_FIT };
	for (auto& strategy : strategies)
	{
		HeapHelper<int> intHelper;
		HeapHelper<float> floatHelper;
		HeapHelper<std::string> stringHelper;
		intHelper.Initialize(1000 * sizeof(int));
		floatHelper.Initialize(1000 * sizeof(float));
		stringHelper.Initialize(1000 * sizeof(std::string));

		for (size_t i = 0; i < 1000; ++i)
		{
			TestAllocation(intHelper, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelper, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelper, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			TestDeallocation(intHelper, i);
			TestDeallocation(floatHelper, i);
			TestDeallocation(stringHelper, i);
		}
	}
}

TEST(HeapHelperTest, HandleSimpleCombinedAllocationsAndDeallocations)
{
	std::array<AllocationStrategy, 3> strategies = {
		AllocationStrategy::FIRST_FIT, AllocationStrategy::BEST_FIT,
		AllocationStrategy::WORST_FIT };
	for (auto& strategy : strategies)
	{
		HeapHelper<int> intHelper;
		HeapHelper<float> floatHelper;
		HeapHelper<std::string> stringHelper;
		intHelper.Initialize(1000 * sizeof(int));
		floatHelper.Initialize(1000 * sizeof(float));
		stringHelper.Initialize(1000 * sizeof(std::string));

		for (size_t i = 0; i < 1000; ++i)
		{
			TestAllocation(intHelper, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelper, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelper, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			TestDeallocation(intHelper, i);
			TestDeallocation(floatHelper, i);
			TestDeallocation(stringHelper, i);
		}

		TestAllocation(intHelper, sizeof(int), strategy,
			alignof(int), 0, 0);
		TestAllocation(floatHelper, sizeof(float), strategy,
			alignof(float), 0, 0);
		TestAllocation(stringHelper, sizeof(std::string), strategy,
			alignof(std::string), 0, 0);

		for (size_t i = 1; i < 1000; ++i)
		{
			TestAllocation(intHelper, sizeof(int), strategy,
				alignof(int), 1000 - i, i * sizeof(int));
			TestAllocation(floatHelper, sizeof(float), strategy,
				alignof(float), 1000 - i, i * sizeof(float));
			TestAllocation(stringHelper, sizeof(std::string), strategy,
				alignof(std::string), 1000 - i, i * sizeof(std::string));
		}
	}
}

TEST(HeapHelperTest, HandlesAdvancedCombinedAllocationsAndDeallocations)
{
	AllocationStrategy strategy = AllocationStrategy::BEST_FIT;
	HeapHelper<int> intHelper;
	HeapHelper<float> floatHelper;
	HeapHelper<std::string> stringHelper;
	intHelper.Initialize(1000);
	floatHelper.Initialize(1000);
	stringHelper.Initialize(1000);
	size_t expectedOffset = 0;

	TestAllocation(intHelper, 33, strategy, 1, 0, 0);
	TestAllocation(floatHelper, 33, strategy, 1, 0, 0);
	TestAllocation(stringHelper, 33, strategy, 1, 0, 0);

	TestAllocation(intHelper, 5, strategy, 1, 1, 33);
	TestAllocation(floatHelper, 5, strategy, 1, 1, 33);
	TestAllocation(stringHelper, 5, strategy, 1, 1, 33);

	TestAllocation(intHelper, 128, strategy, 256, 2, 256);
	TestAllocation(floatHelper, 128, strategy, 256, 2, 256);
	TestAllocation(stringHelper, 128, strategy, 256, 2, 256);

	TestAllocation(intHelper, 16, strategy, 64, 3, 64);
	TestAllocation(floatHelper, 16, strategy, 64, 3, 64);
	TestAllocation(stringHelper, 16, strategy, 64, 3, 64);

	TestAllocation(intHelper, 32, strategy, 256, 4, 512);
	TestAllocation(floatHelper, 32, strategy, 256, 4, 512);
	TestAllocation(stringHelper, 32, strategy, 256, 4, 512);

	TestAllocation(intHelper, 144, strategy, 32, 6, 96);
	TestAllocation(floatHelper, 144, strategy, 32, 6, 96);
	TestAllocation(stringHelper, 144, strategy, 32, 6, 96);

	TestDeallocation(intHelper, 0);
	TestDeallocation(floatHelper, 0);
	TestDeallocation(stringHelper, 0);
	TestAllocation(intHelper, 28, strategy, 1, 0, 0);
	TestAllocation(floatHelper, 28, strategy, 1, 0, 0);
	TestAllocation(stringHelper, 28, strategy, 1, 0, 0);
	TestAllocation(intHelper, 5, strategy, 1, 11, 28);
	TestAllocation(floatHelper, 5, strategy, 1, 11, 28);
	TestAllocation(stringHelper, 5, strategy, 1, 11, 28);

	TestDeallocation(intHelper, 3);
	TestDeallocation(floatHelper, 3);
	TestDeallocation(stringHelper, 3);
	TestAllocation(intHelper, 58, strategy, 1, 5, 38);
	TestAllocation(floatHelper, 58, strategy, 1, 5, 38);
	TestAllocation(stringHelper, 58, strategy, 1, 5, 38);

	TestAllocation(intHelper, 64, strategy, 256, 8, 768);
	TestAllocation(floatHelper, 64, strategy, 256, 8, 768);
	TestAllocation(stringHelper, 64, strategy, 256, 8, 768);
	TestDeallocation(intHelper, 8);
	TestDeallocation(floatHelper, 8);
	TestDeallocation(stringHelper, 8);
	TestAllocation(intHelper, 64, strategy, 256, 9, 768);
	TestAllocation(floatHelper, 64, strategy, 256, 9, 768);
	TestAllocation(stringHelper, 64, strategy, 256, 9, 768);
}

template<typename T>
void CompareMovedHelpers(const HeapHelper<T>& toCompareTo, const HeapHelper<T>& movedTo,
	const HeapHelper<T>& movedFrom, size_t expectedNrOfChunks)
{
	ASSERT_EQ(toCompareTo.TotalSize(), movedTo.TotalSize());
	ASSERT_EQ(movedFrom.TotalSize(), 0);

	for (size_t i = 0; i < expectedNrOfChunks; ++i)
	{
		ASSERT_EQ(toCompareTo.GetStartOfChunk(i), movedTo.GetStartOfChunk(i));
	}
}

TEST(HeapHelperTest, MoveConstructsCorrectly)
{
	std::array<AllocationStrategy, 3> strategies = {
		AllocationStrategy::FIRST_FIT, AllocationStrategy::BEST_FIT,
		AllocationStrategy::WORST_FIT };
	for (auto& strategy : strategies)
	{
		HeapHelper<int> intHelperToCompareAgainst;
		HeapHelper<float> floatHelperToCompareAgainst;
		HeapHelper<std::string> stringHelperToCompareAgainst;
		intHelperToCompareAgainst.Initialize(1000 * sizeof(int));
		floatHelperToCompareAgainst.Initialize(1000 * sizeof(float));
		stringHelperToCompareAgainst.Initialize(1000 * sizeof(std::string));

		HeapHelper<int> intHelperToMoveFrom;
		HeapHelper<float> floatHelperToMoveFrom;
		HeapHelper<std::string> stringHelperToMoveFrom;
		intHelperToMoveFrom.Initialize(1000 * sizeof(int));
		floatHelperToMoveFrom.Initialize(1000 * sizeof(float));
		stringHelperToMoveFrom.Initialize(1000 * sizeof(std::string));

		for (size_t i = 0; i < 1000; ++i)
		{
			TestAllocation(intHelperToCompareAgainst, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelperToCompareAgainst, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelperToCompareAgainst, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));

			TestAllocation(intHelperToMoveFrom, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelperToMoveFrom, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelperToMoveFrom, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			TestDeallocation(intHelperToCompareAgainst, i);
			TestDeallocation(floatHelperToCompareAgainst, i);
			TestDeallocation(stringHelperToCompareAgainst, i);

			TestDeallocation(intHelperToMoveFrom, i);
			TestDeallocation(floatHelperToMoveFrom, i);
			TestDeallocation(stringHelperToMoveFrom, i);
		}

		TestAllocation(intHelperToCompareAgainst, sizeof(int), strategy,
			alignof(int), 0, 0);
		TestAllocation(floatHelperToCompareAgainst, sizeof(float), strategy,
			alignof(float), 0, 0);
		TestAllocation(stringHelperToCompareAgainst, sizeof(std::string), strategy,
			alignof(std::string), 0, 0);

		TestAllocation(intHelperToMoveFrom, sizeof(int), strategy,
			alignof(int), 0, 0);
		TestAllocation(floatHelperToMoveFrom, sizeof(float), strategy,
			alignof(float), 0, 0);
		TestAllocation(stringHelperToMoveFrom, sizeof(std::string), strategy,
			alignof(std::string), 0, 0);

		for (size_t i = 1; i < 1000; ++i)
		{
			TestAllocation(intHelperToCompareAgainst, sizeof(int), strategy,
				alignof(int), 1000 - i, i * sizeof(int));
			TestAllocation(floatHelperToCompareAgainst, sizeof(float), strategy,
				alignof(float), 1000 - i, i * sizeof(float));
			TestAllocation(stringHelperToCompareAgainst, sizeof(std::string), strategy,
				alignof(std::string), 1000 - i, i * sizeof(std::string));

			TestAllocation(intHelperToMoveFrom, sizeof(int), strategy,
				alignof(int), 1000 - i, i * sizeof(int));
			TestAllocation(floatHelperToMoveFrom, sizeof(float), strategy,
				alignof(float), 1000 - i, i * sizeof(float));
			TestAllocation(stringHelperToMoveFrom, sizeof(std::string), strategy,
				alignof(std::string), 1000 - i, i * sizeof(std::string));
		}

		HeapHelper<int> intHelperToMoveTo = std::move(intHelperToMoveFrom);
		HeapHelper<float> floatHelperToMoveTo = std::move(floatHelperToMoveFrom);
		HeapHelper<std::string> stringHelperToMoveTo = std::move(stringHelperToMoveFrom);

		CompareMovedHelpers(intHelperToCompareAgainst, intHelperToMoveTo, intHelperToMoveFrom, 1000);
		CompareMovedHelpers(floatHelperToCompareAgainst, floatHelperToMoveTo, floatHelperToMoveFrom, 1000);
		CompareMovedHelpers(stringHelperToCompareAgainst, stringHelperToMoveTo, stringHelperToMoveFrom, 1000);
	}
}

TEST(HeapHelperTest, MoveAssignsCorrectly)
{
	std::array<AllocationStrategy, 3> strategies = {
		AllocationStrategy::FIRST_FIT, AllocationStrategy::BEST_FIT,
		AllocationStrategy::WORST_FIT };
	for (auto& strategy : strategies)
	{
		HeapHelper<int> intHelperToCompareAgainst;
		HeapHelper<float> floatHelperToCompareAgainst;
		HeapHelper<std::string> stringHelperToCompareAgainst;
		intHelperToCompareAgainst.Initialize(1000 * sizeof(int));
		floatHelperToCompareAgainst.Initialize(1000 * sizeof(float));
		stringHelperToCompareAgainst.Initialize(1000 * sizeof(std::string));

		HeapHelper<int> intHelperToMoveFrom;
		HeapHelper<float> floatHelperToMoveFrom;
		HeapHelper<std::string> stringHelperToMoveFrom;
		intHelperToMoveFrom.Initialize(1000 * sizeof(int));
		floatHelperToMoveFrom.Initialize(1000 * sizeof(float));
		stringHelperToMoveFrom.Initialize(1000 * sizeof(std::string));

		HeapHelper<int> intHelperToMoveTo;
		HeapHelper<float> floatHelperToMoveTo;
		HeapHelper<std::string> stringHelperToMoveTo;

		for (size_t i = 0; i < 1000; ++i)
		{
			TestAllocation(intHelperToCompareAgainst, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelperToCompareAgainst, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelperToCompareAgainst, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));

			TestAllocation(intHelperToMoveFrom, sizeof(int), strategy,
				alignof(int), i, i * sizeof(int));
			TestAllocation(floatHelperToMoveFrom, sizeof(float), strategy,
				alignof(float), i, i * sizeof(float));
			TestAllocation(stringHelperToMoveFrom, sizeof(std::string), strategy,
				alignof(std::string), i, i * sizeof(std::string));
		}

		for (size_t i = 0; i < 1000; ++i)
		{
			TestDeallocation(intHelperToCompareAgainst, i);
			TestDeallocation(floatHelperToCompareAgainst, i);
			TestDeallocation(stringHelperToCompareAgainst, i);

			TestDeallocation(intHelperToMoveFrom, i);
			TestDeallocation(floatHelperToMoveFrom, i);
			TestDeallocation(stringHelperToMoveFrom, i);
		}

		TestAllocation(intHelperToCompareAgainst, sizeof(int), strategy,
			alignof(int), 0, 0);
		TestAllocation(floatHelperToCompareAgainst, sizeof(float), strategy,
			alignof(float), 0, 0);
		TestAllocation(stringHelperToCompareAgainst, sizeof(std::string), strategy,
			alignof(std::string), 0, 0);

		TestAllocation(intHelperToMoveFrom, sizeof(int), strategy,
			alignof(int), 0, 0);
		TestAllocation(floatHelperToMoveFrom, sizeof(float), strategy,
			alignof(float), 0, 0);
		TestAllocation(stringHelperToMoveFrom, sizeof(std::string), strategy,
			alignof(std::string), 0, 0);

		for (size_t i = 1; i < 1000; ++i)
		{
			TestAllocation(intHelperToCompareAgainst, sizeof(int), strategy,
				alignof(int), 1000 - i, i * sizeof(int));
			TestAllocation(floatHelperToCompareAgainst, sizeof(float), strategy,
				alignof(float), 1000 - i, i * sizeof(float));
			TestAllocation(stringHelperToCompareAgainst, sizeof(std::string), strategy,
				alignof(std::string), 1000 - i, i * sizeof(std::string));

			TestAllocation(intHelperToMoveFrom, sizeof(int), strategy,
				alignof(int), 1000 - i, i * sizeof(int));
			TestAllocation(floatHelperToMoveFrom, sizeof(float), strategy,
				alignof(float), 1000 - i, i * sizeof(float));
			TestAllocation(stringHelperToMoveFrom, sizeof(std::string), strategy,
				alignof(std::string), 1000 - i, i * sizeof(std::string));
		}

		intHelperToMoveTo = std::move(intHelperToMoveFrom);
		floatHelperToMoveTo = std::move(floatHelperToMoveFrom);
		stringHelperToMoveTo = std::move(stringHelperToMoveFrom);

		CompareMovedHelpers(intHelperToCompareAgainst, intHelperToMoveTo, intHelperToMoveFrom, 1000);
		CompareMovedHelpers(floatHelperToCompareAgainst, floatHelperToMoveTo, floatHelperToMoveFrom, 1000);
		CompareMovedHelpers(stringHelperToCompareAgainst, stringHelperToMoveTo, stringHelperToMoveFrom, 1000);
	}
}