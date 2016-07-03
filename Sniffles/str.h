#ifndef _STR_UTILS_
#define _STR_UTILS_

#include <string.h>
#include <strstream>

#include "mem.h"
#include "err.h"

#include "generated_proto/netmessages_public.pb.h"
#include "generated_proto/cstrike15_usermessages_public.pb.h"

#define out(a)		fputs(a, stdout)
#define outf(...)	printf(__VA_ARGS__)

static void MsgPrintf(const ::google::protobuf::Message& msg, int size, const char *fmt, ...)
{
	va_list vlist;
	const std::string& TypeName = msg.GetTypeName();

	// Print the message type and size
	printf("---- %s (%d bytes) -----------------\n", TypeName.c_str(), size);

	va_start(vlist, fmt);
	vprintf(fmt, vlist);
	va_end(vlist);
}

struct string_t
{
public:
	string_t() { pszValue = 0; }
	
	~string_t() { delete[] pszValue; }

	string_t(const string_t& str) {
		rsize_t size = strlen(str.ToCStr()) + 1;
		pszValue = (char*)malloc(size);
		strcpy_s(pszValue, size, str.ToCStr());
	}

	string_t(const char* str) {
		rsize_t size = strlen(str) + 1;
		pszValue = (char*)malloc(size);
		strcpy_s(pszValue, size, str);
	}

	bool operator!() const { return (pszValue == 0); }
	bool operator==(const string_t &rhs) const { return (pszValue == rhs.pszValue); }
	bool operator!=(const string_t &rhs) const { return (pszValue != rhs.pszValue); }
	bool operator<(const string_t &rhs) const { return ((void *)pszValue < (void *)rhs.pszValue); }

	const char *ToCStr() const { return (pszValue) ? pszValue : ""; }

private:
	char *pszValue;
};

class String
{
public:
	String()
	{
		_length = 0;
		_size = 0;
		_string = 0;
	}

	String(const char* str)
	{
		_length = strlen(str);
		_size = _length + 1;
		_string = new char[_size];
		strcpy_s(_string, _size, str);
	}

	String(char* str)
	{
		_length = strlen(str);
		_size = _length + 1;
		_string = new char[_size];
		strcpy_s(_string, _size, str);
	}

	~String()
	{
		if (_string != 0) {
			delete[] _string;
			_string = 0;
		}
	}

	String& operator+(char* rhs)
	{
		String* temp = new String();

		temp->_length = strlen(rhs) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs);

		return (String&)*temp;
	}

	String& operator+(const char* rhs)
	{
		String* temp = new String();

		temp->_length = strlen(rhs) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs);

		return (String&)*temp;
	}

	String& operator+(String& rhs)
	{
		String* temp = new String();

		temp->_length = strlen(rhs.CStr()) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs.CStr());

		return (String&)*temp;
	}

	String& operator+(int num)
	{
		String* temp = new String();

		char rhs[256];
		_itoa_s(num, rhs, 10);

		temp->_length = strlen(rhs) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs);

		return (String&)*temp;
	}

	String& operator+=(char* rhs)
	{
		String* temp = new String();

		temp->_length = strlen(rhs) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs);

		_length = temp->_length;
		_size = temp->_size;
		_string = temp->_string;

		return (String&)*temp;
	}

	String& operator+=(const char* rhs)
	{
		String* temp = new String();

		temp->_length = strlen(rhs) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs);

		_length = temp->_length;
		_size = temp->_size;
		_string = temp->_string;

		return (String&)*temp;
	}

	String& operator+=(String& rhs)
	{
		String* temp = new String();

		temp->_length = strlen(rhs.CStr()) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strcpy_s(temp->_string, temp->_size, _string);
		strcat_s(temp->_string, temp->_size, rhs.CStr());

		_length = temp->_length;
		_size = temp->_size;
		_string = temp->_string;

		return (String&)*temp;
	}

	String& operator+=(int num)
	{
		String* temp = new String();

		char rhs[256];
		_itoa_s(num, rhs, 10);

		temp->_length = strlen(rhs) + strlen(_string);
		temp->_size = temp->_length + 1;
		temp->_string = new char[temp->_size];

		strncpy_s(temp->_string, temp->_size, _string, temp->_size);
		strcat_s(temp->_string, temp->_size, rhs);

		_length = temp->_length;
		_size = temp->_size;
		_string = temp->_string;

		return (String&)*temp;
	}

	char& operator[](int index)
	{
		if (index >= 0 && index < _size)
		{
			return _string[index];
		}
		else
		{
			shout_error("String[] subscript out of range");
			return _string[0];
		}
	}

	size_t find(String& P)
	{
		size_t n = _length;
		size_t m = P.length();
		for (size_t i = 0; i <= n - m; ++i)
		{
			size_t j = 0;
			while (j < m && _string[i + j] == P[j])
				++j;

			if (j == m) // match found
				return i;
		}
		return npos;
	}

	size_t find(const char* P)
	{
		size_t n = _length;
		size_t m = strlen(P);
		for (size_t i = 0; i <= n - m; ++i)
		{
			size_t j = 0;
			while (j < m && _string[i + j] == P[j])
				++j;

			if (j == m) // match found
				return i;
		}
		return npos;
	}

	size_t length() { return _length; }
	const char *CStr() const { return _string ? _string : ""; }

private:

	//String& _IntToStr(int num)
	//{
	//	char buffer[256];
	//	_itoa_s(num, buffer, 10);
	//	return String(buffer);
	//}

	char* _string;
	int _size;
	size_t _length;

	static const size_t npos;	// bad/missing length/position
};

const size_t String::npos = -1;


namespace StrUtils
{

	template <typename T>
	__inline String NumToStr(T num)
	{
		char buffer[256];
		_itoa_s(num, buffer, 10);
		return String(buffer);
	}

	template <typename T>
	__inline T StrToNum(const String &Text)
	{
		std::istringstream ss(Text.CStr());
		T result;
		return ss >> result ? result : 0;
	}

}

#endif