// This file is part of Notepad++ project
// Copyright (C)2026 Notepad++ Linux port contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Notepad++ Linux plugin SDK: the CString of ATL that plugins use (CStringW, CStringA),
// a std::basic_string with the ATL member functions. char strings are UTF-8 (CP_ACP).

#pragma once

#include <atlbase.h>

#include <algorithm>
#include <cstdarg>
#include <cwctype>
#include <string>

namespace npp_atl
{
	inline std::wstring widen(const char* s)
	{
		if (!s || !*s)
			return std::wstring();
		const int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
		std::wstring w(n > 0 ? n - 1 : 0, L'\0');
		if (n > 1)
			MultiByteToWideChar(CP_ACP, 0, s, -1, &w[0], n);
		return w;
	}

	inline std::string narrow(const wchar_t* s)
	{
		if (!s || !*s)
			return std::string();
		const int n = WideCharToMultiByte(CP_ACP, 0, s, -1, nullptr, 0, nullptr, nullptr);
		std::string a(n > 0 ? n - 1 : 0, '\0');
		if (n > 1)
			WideCharToMultiByte(CP_ACP, 0, s, -1, &a[0], n, nullptr, nullptr);
		return a;
	}

	template <typename C> struct Traits;
	template <> struct Traits<wchar_t>
	{
		static std::wstring from(const char* s) { return widen(s); }
		static std::wstring from(const wchar_t* s) { return s ? s : L""; }
		static size_t length(const wchar_t* s) { return wcslen(s); }
		static wchar_t upper(wchar_t c) { return static_cast<wchar_t>(towupper(c)); }
		static wchar_t lower(wchar_t c) { return static_cast<wchar_t>(towlower(c)); }
		static bool space(wchar_t c) { return iswspace(c) != 0; }
		static std::wstring vformat(const wchar_t* format, va_list args)
		{
			va_list copy;
			va_copy(copy, args);
			int n = npp_vsnwprintf(nullptr, 0, format, copy);
			va_end(copy);
			if (n < 0)
				n = 4096;
			std::wstring s(static_cast<size_t>(n) + 1, L'\0');
			n = npp_vsnwprintf(&s[0], s.size(), format, args);
			s.resize(n < 0 ? 0 : static_cast<size_t>(std::min<int>(n, static_cast<int>(s.size()) - 1)));
			return s;
		}
	};
	template <> struct Traits<char>
	{
		static std::string from(const char* s) { return s ? s : ""; }
		static std::string from(const wchar_t* s) { return narrow(s); }
		static size_t length(const char* s) { return strlen(s); }
		static char upper(char c) { return static_cast<char>(toupper(static_cast<unsigned char>(c))); }
		static char lower(char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); }
		static bool space(char c) { return isspace(static_cast<unsigned char>(c)) != 0; }
		static std::string vformat(const char* format, va_list args)
		{
			va_list copy;
			va_copy(copy, args);
			const int n = vsnprintf(nullptr, 0, format, copy);
			va_end(copy);
			std::string s(n > 0 ? static_cast<size_t>(n) + 1 : 1, '\0');
			vsnprintf(&s[0], s.size(), format, args);
			s.resize(n > 0 ? static_cast<size_t>(n) : 0);
			return s;
		}
	};

	template <typename C> class CStringT : public std::basic_string<C>
	{
		using Base = std::basic_string<C>;
		using T = Traits<C>;
	public:
		using XCHAR = C;
		CStringT() = default;
		CStringT(const char* s) : Base(T::from(s)) {}
		CStringT(const wchar_t* s) : Base(T::from(s)) {}
		CStringT(const C* s, int length) : Base(s, static_cast<size_t>(length)) {}
		CStringT(C c, int repeat = 1) : Base(static_cast<size_t>(repeat), c) {}
		CStringT(const Base& s) : Base(s) {}
		CStringT(Base&& s) : Base(std::move(s)) {}

		CStringT& operator=(const char* s) { Base::operator=(T::from(s)); return *this; }
		CStringT& operator=(const wchar_t* s) { Base::operator=(T::from(s)); return *this; }
		CStringT& operator=(C c) { Base::assign(1, c); return *this; }
		CStringT& operator=(const Base& s) { Base::operator=(s); return *this; }
		CStringT& operator+=(const char* s) { Base::append(T::from(s)); return *this; }
		CStringT& operator+=(const wchar_t* s) { Base::append(T::from(s)); return *this; }
		CStringT& operator+=(C c) { Base::push_back(c); return *this; }
		CStringT& operator+=(const Base& s) { Base::append(s); return *this; }

		operator const C*() const { return this->c_str(); }
		C operator[](int i) const { return Base::operator[](static_cast<size_t>(i)); }
		C GetAt(int i) const { return Base::at(static_cast<size_t>(i)); }
		void SetAt(int i, C c) { Base::at(static_cast<size_t>(i)) = c; }
		const C* GetString() const { return this->c_str(); }
		int GetLength() const { return static_cast<int>(this->size()); }
		bool IsEmpty() const { return this->empty(); }
		void Empty() { this->clear(); }
		void Append(const C* s) { if (s) Base::append(s); }
		void AppendChar(C c) { Base::push_back(c); }

		C* GetBuffer(int minLength = 0)
		{
			if (minLength > GetLength())
				this->resize(static_cast<size_t>(minLength));
			return Base::data();
		}
		C* GetBufferSetLength(int length) { this->resize(static_cast<size_t>(length)); return Base::data(); }
		void ReleaseBuffer(int newLength = -1)
		{
			this->resize(newLength < 0 ? T::length(this->c_str()) : static_cast<size_t>(newLength));
		}

		CStringT Mid(int first, int count = -1) const
		{
			if (first < 0)
				first = 0;
			if (first >= GetLength())
				return CStringT();
			return CStringT(Base::substr(static_cast<size_t>(first), count < 0 ? Base::npos : static_cast<size_t>(count)));
		}
		CStringT Left(int count) const { return Mid(0, count < 0 ? 0 : count); }
		CStringT Right(int count) const
		{
			count = std::max(0, std::min(count, GetLength()));
			return Mid(GetLength() - count);
		}
		int Find(C c, int start = 0) const { return pos(Base::find(c, static_cast<size_t>(std::max(start, 0)))); }
		int Find(const C* s, int start = 0) const { return pos(Base::find(s, static_cast<size_t>(std::max(start, 0)))); }
		// the token from start on (delimiters before it skipped), start after it; -1 at the end
		CStringT Tokenize(const C* delimiters, int& start) const
		{
			if (start < 0 || start >= GetLength())
			{
				start = -1;
				return CStringT();
			}
			const size_t first = Base::find_first_not_of(delimiters, static_cast<size_t>(start));
			if (first == Base::npos)
			{
				start = -1;
				return CStringT();
			}
			size_t last = Base::find_first_of(delimiters, first);
			if (last == Base::npos)
				last = this->size();
			start = static_cast<int>(last) + 1;
			return CStringT(Base::substr(first, last - first));
		}
		int ReverseFind(C c) const { return pos(Base::rfind(c)); }
		int FindOneOf(const C* set) const { return pos(Base::find_first_of(set)); }
		int Replace(const C* from, const C* to)
		{
			const Base f(from), t(to);
			if (f.empty())
				return 0;
			int n = 0;
			for (size_t i = Base::find(f); i != Base::npos; i = Base::find(f, i + t.size()))
			{
				Base::replace(i, f.size(), t);
				++n;
			}
			return n;
		}
		int Replace(C from, C to)
		{
			int n = 0;
			for (C& c : *this)
			{
				if (c == from)
				{
					c = to;
					++n;
				}
			}
			return n;
		}
		int Remove(C c)
		{
			const int n = static_cast<int>(std::count(this->begin(), this->end(), c));
			Base::erase(std::remove(this->begin(), this->end(), c), this->end());
			return n;
		}
		int Delete(int index, int count = 1)
		{
			if (index >= 0 && index < GetLength())
				Base::erase(static_cast<size_t>(index), static_cast<size_t>(count));
			return GetLength();
		}
		int Insert(int index, const C* s) { Base::insert(static_cast<size_t>(std::min(std::max(index, 0), GetLength())), s); return GetLength(); }
		int Insert(int index, C c) { Base::insert(static_cast<size_t>(std::min(std::max(index, 0), GetLength())), 1, c); return GetLength(); }
		CStringT& MakeUpper() { for (C& c : *this) c = T::upper(c); return *this; }
		CStringT& MakeLower() { for (C& c : *this) c = T::lower(c); return *this; }
		CStringT& MakeReverse() { std::reverse(this->begin(), this->end()); return *this; }
		CStringT& TrimLeft() { size_t i = 0; while (i < this->size() && T::space((*this)[i])) ++i; Base::erase(0, i); return *this; }
		CStringT& TrimRight() { while (!this->empty() && T::space(this->back())) this->pop_back(); return *this; }
		CStringT& Trim() { TrimRight(); return TrimLeft(); }
		int Compare(const C* s) const { return Base::compare(s); }
		int CompareNoCase(const C* s) const
		{
			CStringT a(*this), b(s);
			return a.MakeLower().Base::compare(b.MakeLower());
		}
		void Format(const C* format, ...)
		{
			va_list ap;
			va_start(ap, format);
			Base::operator=(T::vformat(format, ap));
			va_end(ap);
		}
		void AppendFormat(const C* format, ...)
		{
			va_list ap;
			va_start(ap, format);
			Base::append(T::vformat(format, ap));
			va_end(ap);
		}

	private:
		static int pos(size_t p) { return p == Base::npos ? -1 : static_cast<int>(p); }
	};

	template <typename C> CStringT<C> operator+(const CStringT<C>& a, const CStringT<C>& b) { CStringT<C> r(a); r += b; return r; }
	template <typename C> CStringT<C> operator+(const CStringT<C>& a, const C* b) { CStringT<C> r(a); r += b; return r; }
	template <typename C> CStringT<C> operator+(const C* a, const CStringT<C>& b) { CStringT<C> r(a); r += b; return r; }
	template <typename C> CStringT<C> operator+(const CStringT<C>& a, C b) { CStringT<C> r(a); r += b; return r; }
}

typedef npp_atl::CStringT<wchar_t> CStringW;
typedef npp_atl::CStringT<char> CStringA;
typedef CStringW CString;
