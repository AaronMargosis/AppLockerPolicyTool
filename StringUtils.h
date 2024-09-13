#pragma once

#include <string>
#include <sstream>
#include <vector>

// ------------------------------------------------------------------------------------------
// StartsWith, EndsWith, SplitStringToVector

/// <summary>
/// Returns true if the input string "str" starts with the input string "with".
/// Case insensitive by default, can be overridden.
/// </summary>
/// <param name="str">Input: string to inspect</param>
/// <param name="with">Input: substring to test str with</param>
/// <param name="bCaseSensitive">Input: true for case sensitive; false for case-insensitive compare</param>
/// <returns>true if the input string "str" starts with the input string "with"</returns>
inline bool StartsWith(const std::wstring& str, const std::wstring& with, bool bCaseSensitive = false)
{
	if (bCaseSensitive)
	{
		return (0 == wcsncmp(str.c_str(), with.c_str(), with.length()));
	}
	else
	{
		return (0 == _wcsnicmp(str.c_str(), with.c_str(), with.length()));
	}
}

/// <summary>
/// Returns true if the input string "str" ends with the character "chr".
/// </summary>
inline bool EndsWith(const std::wstring& str, wchar_t chr)
{
	size_t len = str.length();
	return (len > 0 && str[len - 1] == chr);
}

/// <summary>
/// Similar to .NET's string split method, returns a vector of substrings of the input string based
/// on the supplied delimiter.
/// </summary>
/// <param name="strInput">Input: string from which to return substrings</param>
/// <param name="delim">Input: delimiter character to separate substrings</param>
/// <param name="elems">Output: vector of substrings</param>
void SplitStringToVector(const std::wstring& strInput, wchar_t delim, std::vector<std::wstring>& elems);

// ------------------------------------------------------------------------------------------
/// <summary>
/// Convert a wstring in place to locale-sensitive upper-case
/// </summary>
/// <param name="str"></param>
/// <returns></returns>
std::wstring& WString_To_Upper(std::wstring& str);

// ------------------------------------------------------------------------------------------
// Replace all instances of one substring with another (std::wstring and std::string)

/// <summary>
/// Replace all instances of one substring with another.
/// </summary>
/// <param name="str">The original string</param>
/// <param name="replace">The substring to search for</param>
/// <param name="with">The substring to put into the result in place of the searched-for substring</param>
/// <returns>The modified string</returns>
inline std::wstring replaceStringAll(std::wstring str,
    const std::wstring& replace,
    const std::wstring& with) {
    if (!replace.empty()) {
        std::size_t pos = 0;
        while ((pos = str.find(replace, pos)) != std::string::npos) {
            str.replace(pos, replace.length(), with);
            pos += with.length();
        }
    }
    return str;
}

inline std::string replaceStringAll(std::string str,
    const std::string& replace,
    const std::string& with) {
    if (!replace.empty()) {
        std::size_t pos = 0;
        while ((pos = str.find(replace, pos)) != std::string::npos) {
            str.replace(pos, replace.length(), with);
            pos += with.length();
        }
    }
    return str;
}

// ------------------------------------------------------------------------------------------
// Date/time-related string manipulation

/// <summary>
/// Convert input system time structure to an alpha-sortable date/time string, optionally including 
/// milliseconds, and optionally including only characters that are valid in directory and file names.
/// </summary>
/// <param name="st">Input: SYSTEMTIME structure representing the date/time to convert to a string</param>
/// <param name="bIncludeMilliseconds">Input: true to include milliseconds, false otherwise</param>
/// <param name="bForFileSystem">Input: true to limit to file-object-valid characters</param>
/// <returns>Timestamp string with a format like yyyy-MM-dd HH:mm:ss.fff</returns>
std::wstring SystemTimeToWString(const SYSTEMTIME& st, bool bIncludeMilliseconds, bool bForFileSystem = false);

/// <summary>
/// Convert input filetime structure to an alpha-sortable date/time string, optionally including
/// milliseconds and optionally including only characters that are valid in directory and file names.
/// If the filetime value is zero, returns an alternate string value (rather than converting to 1601-01-01).
/// </summary>
/// <param name="ft">Input: FILETIME structure representing the date/time to convert to a string</param>
/// <param name="bIncludeMilliseconds">Input: true to include milliseconds, false otherwise</param>
/// <param name="szIfZero">Input (optional): the string to return if the ft is zero.</param>
/// <returns>Timestamp string with a format like yyyy-MM-dd HH:mm:ss.fff, or alternate string value</returns>
std::wstring FileTimeToWString(const FILETIME& ft, bool bIncludeMilliseconds, const wchar_t* szIfZero = L"");

/// <summary>
/// Converts input LARGE_INTEGER to an alpha-sortable date/time string, where the input value represents
/// the number of 100-nanosecond intervals since January 1, 1601 (UTC).
/// </summary>
/// <param name="l">Input: Number of 100-nanosecond intervals since 1/1/1601</param>
/// <param name="bIncludeMilliseconds">Input: true to include milliseconds, false otherwise</param>
/// <param name="szIfZero">Input (optional): the string to return if the ft is zero.</param>
/// <returns>Timestamp string with a format like yyyy-MM-dd HH:mm:ss.fff, or alternate string value</returns>
std::wstring LargeIntegerToDateTimeString(const LARGE_INTEGER& l, bool bIncludeMilliseconds = true, const wchar_t* szIfZero = L"");

/// <summary>
/// Creates and returns an alpha-sortable timestamp string from the current time, optionally including milliseconds
/// </summary>
/// <param name="bIncludeMilliseconds">Input: whether to incorporate milliseconds in the output</param>
/// <returns>Alpha-sortable timestamp string</returns>
std::wstring TimestampUTC(bool bIncludeMilliseconds = false);

/// <summary>
/// Create and returns an alpha-sortable timestamp string from the current time, optionally including milliseconds,
/// using only characters that are valid in filenames.
/// </summary>
/// <param name="bIncludeMilliseconds">Input: whether to incorporate milliseconds in the output</param>
/// <returns>Alpha-sortable timestamp string</returns>
std::wstring TimestampUTCforFilepath(bool bIncludeMilliseconds = false);


// ------------------------------------------------------------------------------------------

/// <summary>
/// Encodes string for XML. E.g., EncodeForXml(L"<root>") returns "&lt;root&gt;".
/// </summary>
std::wstring EncodeForXml(const wchar_t* sz);


/// <summary>
/// Performs case-insensitive string equality comparison
/// </summary>
/// <param name="str1"></param>
/// <param name="str2"></param>
/// <returns>true if the input strings are the same (case insensitive)</returns>
inline bool EqualCaseInsensitive(const std::wstring& str1, const std::wstring& str2)
{
	return (0 == _wcsicmp(str1.c_str(), str2.c_str()));
}



