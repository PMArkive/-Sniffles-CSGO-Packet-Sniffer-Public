#pragma once

template <typename T, size_t N = 1>
struct memory_t
{
public:
	memory_t()
	{
		init(sizeof(T) * N);
	}

	memory_t(const memory_t& mem)
	{
		init(mem.size());
		memcpy_s(m_pValue, m_nSize, mem.ptr(), m_nSize);
	}

	memory_t(void* ptr, int size)
	{
		init(size * N);
		memcpy_s(m_pValue, m_nSize, ptr, m_nSize);
	}

	memory_t(void* ptr)
	{
		init(sizeof(T) * N)
			memcpy_s(m_pValue, m_nSize, ptr, m_nSize);
	}

	memory_t(T val)
	{
		init(sizeof(T) * N);
		memcpy_s(m_pValue, m_nSize, &val, m_nSize);
	}

	void init(int size)
	{
		m_nSize = size;
		m_pValue = (T*)malloc(m_nSize);
	}

	void free()
	{
		if (m_pValue)
			delete[] m_pValue;
		m_pValue = 0;
		m_nSize = 0;
	}

	bool operator!() const { return (m_pValue == NULL); }
	bool operator==(const memory_t &rhs) const { return (m_pValue == rhs.m_pValue); }
	bool operator!=(const memory_t &rhs) const { return (m_pValue != rhs.m_pValue); }
	bool operator<(const memory_t &rhs) const { return ((void *)m_pValue < (void *)rhs.m_pValue); }

	int size() const { return m_nSize; }
	const T* ptr() const { return (m_pValue) ? m_pValue : NULL; }
	const T value() const { return (m_pValue) ? *m_pValue : NULL; }

private:
	T *m_pValue;
	int m_nSize;
};