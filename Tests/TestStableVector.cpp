#include "pch.h"

#include <string>
#include <utility>

#include "../Neo Steelgear Graphics Core/StableVector.h"

template<typename T>
void TestSizes(const StableVector<T>& vector, size_t activeSize, size_t totalSize)
{
	ASSERT_EQ(vector.ActiveSize(), activeSize);
	ASSERT_EQ(vector.TotalSize(), totalSize);
}

TEST(StableVectorTest, DefaultInitialisable)
{
	StableVector<int> intVector;
	TestSizes(intVector, 0, 0);
	StableVector<float> floatVector;
	TestSizes(floatVector, 0, 0);
	StableVector<std::string> stringVector;
	TestSizes(stringVector, 0, 0);
}

template<typename T>
void TestAddCopy(StableVector<T>& vector, const T& element, 
	size_t expectedTotalSizeAfterAdd, size_t expectedAddIndex)
{
	size_t activeSize = vector.ActiveSize();
	size_t addedIndex = vector.Add(element);
	TestSizes(vector, activeSize + 1, expectedTotalSizeAfterAdd);
	ASSERT_EQ(addedIndex, expectedAddIndex);
	ASSERT_EQ(vector.CheckIfActive(addedIndex), true);
	ASSERT_EQ(vector[addedIndex], element);
}

template<typename T>
void TestAddMove(StableVector<T>& vector, const T& element,
	size_t expectedTotalSizeAfterAdd, size_t expectedAddIndex)
{
	T toCompareTo = element;
	size_t activeSize = vector.ActiveSize();
	size_t addedIndex = vector.Add(std::move(element));
	TestSizes(vector, activeSize + 1, expectedTotalSizeAfterAdd);
	ASSERT_EQ(addedIndex, expectedAddIndex);
	ASSERT_EQ(vector.CheckIfActive(addedIndex), true);
	ASSERT_EQ(vector[addedIndex], toCompareTo);
}

TEST(StableVectorTest, PerformsSimpleAddsCorrectly)
{
	StableVector<int> intVector;
	StableVector<float> floatVector;
	StableVector<std::string> stringVector;

	size_t i = 0;

	for (; i < 1000; ++i)
	{
		TestAddCopy(intVector, static_cast<int>(i), i + 1, i);
		TestAddCopy(floatVector, static_cast<float>(i), i + 1, i);
		TestAddCopy(stringVector, std::to_string(i), i + 1, i);
	}

	for (; i < 2000; ++i)
	{
		TestAddMove(intVector, static_cast<int>(i), i + 1, i);
		TestAddMove(floatVector, static_cast<float>(i), i + 1, i);
		TestAddMove(stringVector, std::to_string(i), i + 1, i);
	}
}

template<typename T>
void TestRemove(StableVector<T>& vector, size_t indexToRemove)
{
	std::vector<std::pair<bool, T>> currentContents;
	currentContents.resize(vector.TotalSize());

	for (size_t i = 0; i < vector.TotalSize(); ++i)
	{
		if (vector.CheckIfActive(i))
			currentContents[i] = std::make_pair(true, vector[i]);
		else
			currentContents[i] = std::make_pair(false, T());
	}

	size_t activeSize = vector.ActiveSize();
	size_t totalSize = vector.TotalSize();
	vector.Remove(indexToRemove);
	TestSizes(vector, activeSize - 1, totalSize);
	ASSERT_EQ(vector.CheckIfActive(indexToRemove), false);

	for (size_t i = 0; i < vector.TotalSize(); ++i)
	{
		if (i == indexToRemove)
			continue;

		if (currentContents[i].first)
		{
			ASSERT_EQ(vector.CheckIfActive(i), true);
			ASSERT_EQ(vector[i], currentContents[i].second);
		}
		else
		{
			ASSERT_EQ(vector.CheckIfActive(i), false);
		}
	}
}

TEST(StableVectorTest, RemovesCorrectly)
{
	StableVector<int> intVector;
	StableVector<float> floatVector;
	StableVector<std::string> stringVector;

	size_t i = 0;

	for (; i < 1000; ++i)
	{
		intVector.Add(static_cast<int>(i));
		floatVector.Add(static_cast<float>(i));
		stringVector.Add(std::to_string(i));
	}

	for (i = 0; i < 100; ++i)
	{
		TestRemove(intVector, i);
		TestRemove(floatVector, i);
		TestRemove(stringVector, i);
	}

	for (; i < 200; i += 2)
	{
		TestRemove(intVector, i);
		TestRemove(floatVector, i);
		TestRemove(stringVector, i);
	}

	for (; i < 1000; i += 10)
	{
		TestRemove(intVector, i);
		TestRemove(floatVector, i);
		TestRemove(stringVector, i);
	}
}

TEST(StableVectorTest, ReusesIndicesCorrectly)
{
	StableVector<int> intVector;
	StableVector<float> floatVector;
	StableVector<std::string> stringVector;

	size_t i = 0;

	for (; i < 1000; ++i)
	{
		intVector.Add(static_cast<int>(i));
		floatVector.Add(static_cast<float>(i));
		stringVector.Add(std::to_string(i));
	}

	for (i = 0; i < 100; ++i)
	{
		intVector.Remove(i);
		floatVector.Remove(i);
		stringVector.Remove(i);
	}

	for (i = 0; i < 10; ++i)
	{
		TestAddCopy(intVector, static_cast<int>(i), 1000, 99 - i);
		TestAddCopy(floatVector, static_cast<float>(i), 1000, 99 - i);
		TestAddCopy(stringVector, std::to_string(i), 1000, 99 - i);
	}

	for (; i < 200; i += 2)
	{
		intVector.Remove(i);
		floatVector.Remove(i);
		stringVector.Remove(i);
	}

	for (i = 100; i < 200; i += 2)
	{
		TestAddCopy(intVector, static_cast<int>(i), 1000, 298 - i);
		TestAddCopy(floatVector, static_cast<float>(i), 1000, 298 - i);
		TestAddCopy(stringVector, std::to_string(i), 1000, 298 - i);
	}

	for (; i < 1000; i += 10)
	{
		intVector.Remove(i);
		floatVector.Remove(i);
		stringVector.Remove(i);
	}

	for (i = 200; i < 1000; i += 10)
	{
		TestAddCopy(intVector, static_cast<int>(i), 1000, 1190 - i);
		TestAddCopy(floatVector, static_cast<float>(i), 1000, 1190 - i);
		TestAddCopy(stringVector, std::to_string(i), 1000, 1190 - i);
	}
}

TEST(StableVectorTest, MoveConstructsCorrectly)
{
	StableVector<int> intVectorToCompareAgainst;
	StableVector<float> floatVectorToCompareAgainst;
	StableVector<std::string> stringVectorToCompareAgainst;

	StableVector<int> intVectorToMoveFrom;
	StableVector<float> floatVectorToMoveFrom;
	StableVector<std::string> stringVectorToMoveFrom;

	size_t i = 0;

	for (; i < 1000; ++i)
	{
		intVectorToCompareAgainst.Add(static_cast<int>(i));
		floatVectorToCompareAgainst.Add(static_cast<float>(i));
		stringVectorToCompareAgainst.Add(std::to_string(i));

		intVectorToMoveFrom.Add(static_cast<int>(i));
		floatVectorToMoveFrom.Add(static_cast<float>(i));
		stringVectorToMoveFrom.Add(std::to_string(i));
	}

	for (i = 0; i < 100; ++i)
	{
		intVectorToCompareAgainst.Remove(i);
		floatVectorToCompareAgainst.Remove(i);
		stringVectorToCompareAgainst.Remove(i);

		intVectorToMoveFrom.Remove(i);
		floatVectorToMoveFrom.Remove(i);
		stringVectorToMoveFrom.Remove(i);
	}

	StableVector<int> intVectorToMoveTo(std::move(intVectorToMoveFrom));
	StableVector<float> floatVectorToMoveTo(std::move(floatVectorToMoveFrom));
	StableVector<std::string> stringVectorToMoveTo(std::move(stringVectorToMoveFrom));

	ASSERT_EQ(intVectorToMoveFrom.ActiveSize(), 0);
	ASSERT_EQ(intVectorToMoveFrom.TotalSize(), 0);
	ASSERT_EQ(floatVectorToMoveFrom.ActiveSize(), 0);
	ASSERT_EQ(floatVectorToMoveFrom.TotalSize(), 0);
	ASSERT_EQ(stringVectorToMoveFrom.ActiveSize(), 0);
	ASSERT_EQ(stringVectorToMoveFrom.TotalSize(), 0);

	ASSERT_EQ(intVectorToMoveTo.ActiveSize(), intVectorToCompareAgainst.ActiveSize());
	ASSERT_EQ(floatVectorToMoveTo.ActiveSize(), floatVectorToCompareAgainst.ActiveSize());
	ASSERT_EQ(stringVectorToMoveTo.ActiveSize(), stringVectorToCompareAgainst.ActiveSize());
	ASSERT_EQ(intVectorToMoveTo.TotalSize(), intVectorToCompareAgainst.TotalSize());
	ASSERT_EQ(floatVectorToMoveTo.TotalSize(), floatVectorToCompareAgainst.TotalSize());
	ASSERT_EQ(stringVectorToMoveTo.TotalSize(), stringVectorToCompareAgainst.TotalSize());

	for (i = 0; i < 1000; ++i)
	{
		ASSERT_EQ(intVectorToMoveTo.CheckIfActive(i), intVectorToCompareAgainst.CheckIfActive(i));
		ASSERT_EQ(floatVectorToMoveTo.CheckIfActive(i), floatVectorToCompareAgainst.CheckIfActive(i));
		ASSERT_EQ(stringVectorToMoveTo.CheckIfActive(i), stringVectorToCompareAgainst.CheckIfActive(i));

		ASSERT_EQ(intVectorToMoveTo[i], intVectorToCompareAgainst[i]);
		ASSERT_EQ(floatVectorToMoveTo[i], floatVectorToCompareAgainst[i]);
		ASSERT_EQ(stringVectorToMoveTo[i], stringVectorToCompareAgainst[i]);
	}
}

TEST(StableVectorTest, MoveAssignsCorrectly)
{
	StableVector<int> intVectorToCompareAgainst;
	StableVector<float> floatVectorToCompareAgainst;
	StableVector<std::string> stringVectorToCompareAgainst;

	StableVector<int> intVectorToMoveFrom;
	StableVector<float> floatVectorToMoveFrom;
	StableVector<std::string> stringVectorToMoveFrom;

	StableVector<int> intVectorToMoveTo;
	StableVector<float> floatVectorToMoveTo;
	StableVector<std::string> stringVectorToMoveTo;

	size_t i = 0;

	for (; i < 1000; ++i)
	{
		intVectorToCompareAgainst.Add(static_cast<int>(i));
		floatVectorToCompareAgainst.Add(static_cast<float>(i));
		stringVectorToCompareAgainst.Add(std::to_string(i));

		intVectorToMoveFrom.Add(static_cast<int>(i));
		floatVectorToMoveFrom.Add(static_cast<float>(i));
		stringVectorToMoveFrom.Add(std::to_string(i));

		intVectorToMoveTo.Add(0);
		floatVectorToMoveTo.Add(0.0f);
		stringVectorToMoveTo.Add(std::to_string(0));
	}

	for (i = 0; i < 100; ++i)
	{
		intVectorToCompareAgainst.Remove(i);
		floatVectorToCompareAgainst.Remove(i);
		stringVectorToCompareAgainst.Remove(i);

		intVectorToMoveFrom.Remove(i);
		floatVectorToMoveFrom.Remove(i);
		stringVectorToMoveFrom.Remove(i);

		intVectorToMoveTo.Remove(i * 2);
		floatVectorToMoveTo.Remove(i * 2);
		stringVectorToMoveTo.Remove(i * 2);
	}

	intVectorToMoveTo = std::move(intVectorToMoveFrom);
	floatVectorToMoveTo = std::move(floatVectorToMoveFrom);
	stringVectorToMoveTo = std::move(stringVectorToMoveFrom);

	ASSERT_EQ(intVectorToMoveFrom.ActiveSize(), 0);
	ASSERT_EQ(intVectorToMoveFrom.TotalSize(), 0);
	ASSERT_EQ(floatVectorToMoveFrom.ActiveSize(), 0);
	ASSERT_EQ(floatVectorToMoveFrom.TotalSize(), 0);
	ASSERT_EQ(stringVectorToMoveFrom.ActiveSize(), 0);
	ASSERT_EQ(stringVectorToMoveFrom.TotalSize(), 0);

	ASSERT_EQ(intVectorToMoveTo.ActiveSize(), intVectorToCompareAgainst.ActiveSize());
	ASSERT_EQ(floatVectorToMoveTo.ActiveSize(), floatVectorToCompareAgainst.ActiveSize());
	ASSERT_EQ(stringVectorToMoveTo.ActiveSize(), stringVectorToCompareAgainst.ActiveSize());
	ASSERT_EQ(intVectorToMoveTo.TotalSize(), intVectorToCompareAgainst.TotalSize());
	ASSERT_EQ(floatVectorToMoveTo.TotalSize(), floatVectorToCompareAgainst.TotalSize());
	ASSERT_EQ(stringVectorToMoveTo.TotalSize(), stringVectorToCompareAgainst.TotalSize());

	for (i = 0; i < 1000; ++i)
	{
		ASSERT_EQ(intVectorToMoveTo.CheckIfActive(i), intVectorToCompareAgainst.CheckIfActive(i));
		ASSERT_EQ(floatVectorToMoveTo.CheckIfActive(i), floatVectorToCompareAgainst.CheckIfActive(i));
		ASSERT_EQ(stringVectorToMoveTo.CheckIfActive(i), stringVectorToCompareAgainst.CheckIfActive(i));

		ASSERT_EQ(intVectorToMoveTo[i], intVectorToCompareAgainst[i]);
		ASSERT_EQ(floatVectorToMoveTo[i], floatVectorToCompareAgainst[i]);
		ASSERT_EQ(stringVectorToMoveTo[i], stringVectorToCompareAgainst[i]);
	}
}