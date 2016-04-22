#pragma once


template<typename T, int InitialCapacity = 1024, int CapcityIncrement = 256>
class fhRenderList {
public:
	fhRenderList()
		: array((T*)R_FrameAlloc(sizeof(T)*InitialCapacity))
		, capacity(InitialCapacity)
		, size(0) {
	}

	void Append(const T& t) {
		assert(size < capacity);
		array[size] = t;
		++size;
	}

	void Clear() {
		size = 0;
	}

	int Num() const {
		return size;
	}

	bool IsEmpty() const {
		return Num() == 0;
	}

	const T& operator[](int i) const {
		assert(i < size);
		return array[i];
	}

private:
	T*  array;
	int capacity;
	int size;
};
