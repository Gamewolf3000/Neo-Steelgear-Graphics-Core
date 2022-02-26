#pragma once

#include <stdexcept>
#include <functional>

#include "StableVector.h"

enum class AllocationStrategy
{
	FIRST_FIT,
	BEST_FIT,
	WORST_FIT
};

template<typename T>
class HeapHelper
{
private:

	enum class ChunkStatus
	{
		AVAILABLE,
		OCCUPIED
	};

	struct Chunk
	{
		ChunkStatus status = ChunkStatus::AVAILABLE;

		size_t actualChunkStart = 0;
		size_t startOffset = 0;
		size_t chunkSize = 0;
		T specificData = T();
	};

	StableVector<Chunk> chunks;
	size_t currentSize;

	void CombineAdjacentChunks(size_t chunkIndex);

	size_t FindFirstFit(size_t dataSize, size_t alignment);
	size_t FindBestFit(size_t dataSize, size_t alignment);
	size_t FindWorstFit(size_t dataSize, size_t alignment);
	size_t FindAvailableChunk(size_t dataSize, AllocationStrategy strategy,
		size_t alignment);

	void SplitChunk(size_t dataSize, size_t alignment, size_t chunkIndex);

	size_t Align(size_t number, size_t alignment);

public:
	HeapHelper() = default;
	~HeapHelper() = default;
	HeapHelper(const HeapHelper& other) = delete;
	HeapHelper& operator=(const HeapHelper& other) = delete;
	HeapHelper(HeapHelper&& other);
	HeapHelper& operator=(HeapHelper&& other);

	void Initialize(size_t heapSize);
	void Initialize(size_t heapSize, const T& specifics);

	size_t AllocateChunk(size_t chunkSize, AllocationStrategy strategy,
		size_t alignment);
	void DeallocateChunk(size_t chunkIndex);

	void AddChunk(size_t chunkSize, bool combine);

	T& operator[](size_t index);

	size_t GetStartOfChunk(size_t index);
	size_t TotalSize();

	void RemoveIf(std::function<bool(const T&)> toCheckWith);
	void ClearHeap(size_t newSize = size_t(-1));
};

template<typename T>
inline void HeapHelper<T>::CombineAdjacentChunks(size_t chunkIndex)
{
	size_t defragStart = chunks[chunkIndex].actualChunkStart;
	size_t defragNext = defragStart + chunks[chunkIndex].chunkSize;

	for (size_t i = 0; i < chunks.TotalSize(); ++i)
	{
		if (chunks.CheckIfActive(i) && chunks[i].status == ChunkStatus::AVAILABLE)
		{
			size_t currentStart = chunks[i].actualChunkStart;
			size_t currentNext = currentStart + chunks[i].chunkSize;

			if (currentStart == defragNext || defragStart == currentNext)
			{
				size_t indexOfFirst = currentStart == defragNext ? chunkIndex : i;
				size_t indexOfSecond = currentStart == defragNext ? i : chunkIndex;

				chunks[indexOfFirst].chunkSize += chunks[indexOfSecond].chunkSize;
				chunks[indexOfSecond].actualChunkStart = size_t(-1);
				chunks[indexOfSecond].chunkSize = 0;
				chunks.Remove(indexOfSecond);

				CombineAdjacentChunks(indexOfFirst);
				return;
			}
		}
	}
}

template<typename T>
inline size_t HeapHelper<T>::FindFirstFit(size_t dataSize, size_t alignment)
{
	for (size_t i = 0; i < chunks.TotalSize(); ++i)
	{
		if (chunks.CheckIfActive(i) && chunks[i].status == ChunkStatus::AVAILABLE)
		{
			size_t alignedAdress = Align(chunks[i].actualChunkStart, alignment);
			size_t alignedSize = chunks[i].chunkSize -
				(alignedAdress - chunks[i].actualChunkStart);

			if (alignedSize >= dataSize)
				return i;
		}
	}

	return size_t(-1);
}

template<typename T>
inline size_t HeapHelper<T>::FindBestFit(size_t dataSize, size_t alignment)
{
	size_t bestIndex = size_t(-1);
	size_t bestSize = size_t(-1);

	for (size_t i = 0; i < chunks.TotalSize(); ++i)
	{
		if (chunks.CheckIfActive(i) && chunks[i].status == ChunkStatus::AVAILABLE)
		{
			size_t alignedAdress = Align(chunks[i].actualChunkStart, alignment);
			size_t alignedSize = chunks[i].chunkSize -
				(alignedAdress - chunks[i].actualChunkStart);

			if (alignedSize >= dataSize && chunks[i].chunkSize < bestSize)
			{
				bestIndex = i;
				bestSize = chunks[i].chunkSize;
			}
		}
	}

	return bestIndex;
}

template<typename T>
inline size_t HeapHelper<T>::FindWorstFit(size_t dataSize, size_t alignment)
{
	size_t bestIndex = size_t(-1);
	size_t worstSize = size_t(-1);

	for (size_t i = 0; i < chunks.TotalSize(); ++i)
	{
		if (chunks.CheckIfActive(i) && chunks[i].status == ChunkStatus::AVAILABLE)
		{
			size_t alignedAdress = Align(chunks[i].actualChunkStart, alignment);
			size_t alignedSize = chunks[i].chunkSize -
				(alignedAdress - chunks[i].actualChunkStart);

			if (alignedSize >= dataSize && chunks[i].chunkSize > worstSize)
			{
				bestIndex = i;
				worstSize = chunks[i].chunkSize;
			}
		}
	}

	return bestIndex;
}

template<typename T>
inline size_t HeapHelper<T>::FindAvailableChunk(size_t dataSize,
	AllocationStrategy strategy, size_t alignment)
{
	size_t chunkIndex;

	switch (strategy)
	{
	case AllocationStrategy::FIRST_FIT:
		chunkIndex = FindFirstFit(dataSize, alignment);
		break;
	case AllocationStrategy::BEST_FIT:
		chunkIndex = FindBestFit(dataSize, alignment);
		break;
	case AllocationStrategy::WORST_FIT:
		chunkIndex = FindWorstFit(dataSize, alignment);
		break;
	default:
		throw std::runtime_error("Error: Incorrect allocation strategy");
	}

	return chunkIndex;
}

template<typename T>
inline void HeapHelper<T>::SplitChunk(size_t dataSize, size_t alignment, 
	size_t chunkIndex)
{
	Chunk remainder;
	size_t alignedAdress = Align(chunks[chunkIndex].actualChunkStart, alignment);
	remainder.startOffset = remainder.actualChunkStart = alignedAdress + dataSize;
	size_t actualSize = alignedAdress - chunks[chunkIndex].actualChunkStart + dataSize;
	remainder.chunkSize = chunks[chunkIndex].chunkSize - actualSize;
	remainder.status = ChunkStatus::AVAILABLE;
	remainder.specificData = T();
	chunks.Add(std::move(remainder));

	chunks[chunkIndex].startOffset = alignedAdress;
	chunks[chunkIndex].chunkSize = actualSize;
	chunks[chunkIndex].status = ChunkStatus::OCCUPIED;
	chunks[chunkIndex].specificData = T();
}

template<typename T>
inline size_t HeapHelper<T>::Align(size_t number, size_t alignment)
{
	if ((0 == alignment) || (alignment & (alignment - 1)))
	{
		throw std::runtime_error("Error: non-pow2 alignment");
	}

	return ((number + (alignment - 1)) & ~(alignment - 1));
}

template<typename T>
inline HeapHelper<T>::HeapHelper(HeapHelper&& other) : 
	chunks(std::move(other.chunks)), currentSize(other.currentSize)
{
	other.currentSize = 0;
}

template<typename T>
inline HeapHelper<T>& HeapHelper<T>::operator=(HeapHelper&& other)
{
	if (this != &other)
	{
		chunks = std::move(other.chunks);
		currentSize = other.currentSize;
		other.currentSize = 0;
	}

	return *this;
}

template<typename T>
inline void HeapHelper<T>::Initialize(size_t heapSize)
{
	Chunk initialChunk;
	initialChunk.startOffset = initialChunk.actualChunkStart = 0;
	initialChunk.chunkSize = heapSize;
	initialChunk.specificData = T();
	currentSize = heapSize;
	chunks.Add(std::move(initialChunk));
}

template<typename T>
inline void HeapHelper<T>::Initialize(size_t heapSize, const T& specifics)
{
	Chunk initialChunk;
	initialChunk.startOffset = initialChunk.actualChunkStart = 0;
	initialChunk.chunkSize = heapSize;
	initialChunk.specificData = specifics;
	currentSize = heapSize;
	chunks.Add(std::move(initialChunk));
}

template<typename T>
inline size_t HeapHelper<T>::AllocateChunk(size_t chunkSize, 
	AllocationStrategy strategy, size_t alignment)
{
	size_t chunkIndex = FindAvailableChunk(chunkSize, strategy, alignment);

	if (chunkIndex != size_t(-1))
	{
		SplitChunk(chunkSize, alignment, chunkIndex);
		chunks[chunkIndex].status = ChunkStatus::OCCUPIED;
	}

	return chunkIndex;
}

template<typename T>
inline void HeapHelper<T>::DeallocateChunk(size_t chunkIndex)
{
	chunks[chunkIndex].status = ChunkStatus::AVAILABLE;
	chunks[chunkIndex].specificData = T();

	CombineAdjacentChunks(chunkIndex);
}

template<typename T>
inline void HeapHelper<T>::AddChunk(size_t chunkSize, bool combine)
{
	Chunk toAdd;
	toAdd.status = ChunkStatus::AVAILABLE;
	toAdd.chunkSize = chunkSize;
	toAdd.actualChunkStart = toAdd.startOffset = currentSize;

	chunks.Add(std::move(toAdd));
	currentSize += chunkSize;

	if (combine)
		CombineAdjacentChunks(chunks.size() - 1);
}

template<typename T>
inline T& HeapHelper<T>::operator[](size_t index)
{
	return chunks[index].specificData;
}

template<typename T>
inline size_t HeapHelper<T>::GetStartOfChunk(size_t index)
{
	return chunks[index].startOffset;
}

template<typename T>
inline size_t HeapHelper<T>::TotalSize()
{
	return currentSize;
}

template<typename T>
inline void HeapHelper<T>::RemoveIf(std::function<bool(const T&)> toCheckWith)
{
	for (size_t i = 0; i < chunks.TotalSize(); ++i)
	{
		if (chunks.CheckIfActive(i) && chunks[i].status == ChunkStatus::OCCUPIED
			&& toCheckWith(chunks[i].specificData))
		{
			DeallocateChunk(i);
		}
	}
}

template<typename T>
inline void HeapHelper<T>::ClearHeap(size_t newSize)
{
	chunks.Clear();

	currentSize = newSize == size_t(-1) ? currentSize : newSize;

	Chunk newTotalChunk;
	newTotalChunk.startOffset = newTotalChunk.actualChunkStart = 0;
	newTotalChunk.chunkSize = currentSize;
	newTotalChunk.specificData = T();
	chunks.Add(std::move(newTotalChunk));
}
