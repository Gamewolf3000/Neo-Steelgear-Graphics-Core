#pragma once

#include <vector>

template<typename T>
class StableVector
{
private:
	struct StoredElement
	{
		bool active = false;
		size_t nextFree = size_t(-1);
		T data;
	};

	std::vector<StoredElement> elements;
	size_t firstFree = size_t(-1);
	size_t nrOfActive = 0;

public:
	StableVector() = default;
	~StableVector() = default;
	StableVector(const StableVector& other) = delete;
	StableVector& operator=(const StableVector& other) = delete;
	StableVector(StableVector&& other) noexcept;
	StableVector& operator=(StableVector&& other) noexcept;

	size_t Add(const T& element);
	size_t Add(T&& element);
	void Remove(size_t index);

	T& operator[](size_t index);

	size_t ActiveSize() const;
	size_t TotalSize() const;
	bool CheckIfActive(size_t index) const;

	void Clear();
};

template<typename T>
inline StableVector<T>::StableVector(StableVector&& other) noexcept :
	elements(std::move(other.elements)), firstFree(other.firstFree),
	nrOfActive(other.nrOfActive)
{
	other.firstFree = size_t(-1);
	other.nrOfActive = 0;
}

template<typename T>
inline StableVector<T>& StableVector<T>::operator=(StableVector&& other) noexcept
{
	if (this != &other)
	{
		elements = std::move(other.elements);
		firstFree = other.firstFree;
		other.firstFree = size_t(-1);
		nrOfActive = other.nrOfActive;
		other.nrOfActive = 0;
	}

	return *this;
}

template<typename T>
inline size_t StableVector<T>::Add(const T& element)
{
	size_t toReturn = size_t(-1);

	StoredElement toAdd;
	toAdd.active = true;
	toAdd.nextFree = size_t(-1);
	toAdd.data = element;

	if (firstFree == size_t(-1))
	{
		toReturn = elements.size();;
		elements.push_back(toAdd);
	}
	else
	{
		size_t nextFree = elements[firstFree].nextFree;
		elements[firstFree] = toAdd;
		toReturn = firstFree;
		firstFree = nextFree;
	}

	++nrOfActive;

	return toReturn;
}

template<typename T>
inline size_t StableVector<T>::Add(T&& element)
{
	size_t toReturn = size_t(-1);

	StoredElement toAdd;
	toAdd.active = true;
	toAdd.nextFree = size_t(-1);
	toAdd.data = std::move(element);

	if (firstFree == size_t(-1))
	{
		toReturn = elements.size();
		elements.push_back(std::move(toAdd));
	}
	else
	{
		size_t nextFree = elements[firstFree].nextFree;
		elements[firstFree] = std::move(toAdd);
		toReturn = firstFree;
		firstFree = nextFree;
	}

	++nrOfActive;

	return toReturn;
}

template<typename T>
inline void StableVector<T>::Remove(size_t index)
{
	elements[index].nextFree = firstFree;
	elements[index].active = false;
	firstFree = index;
	--nrOfActive;
}

template<typename T>
inline T& StableVector<T>::operator[](size_t index)
{
	return elements[index].data;
}

template<typename T>
inline size_t StableVector<T>::ActiveSize() const
{
	return nrOfActive;
}

template<typename T>
inline size_t StableVector<T>::TotalSize() const
{
	return elements.size();
}

template<typename T>
inline bool StableVector<T>::CheckIfActive(size_t index) const
{
	return elements[index].active;
}

template<typename T>
inline void StableVector<T>::Clear()
{
	firstFree = size_t(-1);
	nrOfActive = 0;
	elements.clear();
}