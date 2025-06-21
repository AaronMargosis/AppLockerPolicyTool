#include <Windows.h>
#include <set>

#include "HEX.h"
#include "StringUtils.h"
#include "DirWalker.h"
#include "FileSystemUtils-Windows.h"
#include "GetFilesAndSubdirectories.h"
#include "SysErrorMessage.h"
#include "WindowsDirectories.h"
#include "Wow64FsRedirection.h"
#include "AppLocker_EmergencyClean.h"


// Helper function that gets information about a file or directory and adds it to fileInfoCollection.
static void AddFSObjectToCollection(const std::wstring& sObjName, bool bIsDirectory, FileInfoCollection_t& fileInfoCollection)
{
	FileInfo_t fileInfo;
	fileInfo.bIsDirectory = bIsDirectory;
	fileInfo.sFullPath = sObjName;

	DWORD dwFlags = (bIsDirectory ? FILE_FLAG_BACKUP_SEMANTICS : 0);
	Wow64FsRedirection wow64FSRedir(true);
	HANDLE hFile = CreateFileW(sObjName.c_str(), 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, dwFlags, NULL);
	wow64FSRedir.Revert();
	if (INVALID_HANDLE_VALUE != hFile)
	{
		FILETIME ftCreateTime, ftLastAccessTime, ftLastWriteTime;
		if (GetFileTime(hFile, &ftCreateTime, &ftLastAccessTime, &ftLastWriteTime))
		{
			fileInfo.sCreateTime = FileTimeToWString(ftCreateTime, false);
			fileInfo.sLastWriteTime = FileTimeToWString(ftLastWriteTime, false);
		}
		if (!bIsDirectory)
		{
			GetFileSizeEx(hFile, &fileInfo.filesize);
		}
		CloseHandle(hFile);
	}
	fileInfoCollection.push_back(fileInfo);
}

// Populates a FileInfoCollection_t with information about all files/directories under a directory.
static bool RecursiveFileList(const std::wstring& sDirectory, FileInfoCollection_t& fileInfoCollection)
{
	DirWalker dirWalker;
	std::wstringstream strErrorInfo;
	if (!dirWalker.Initialize(sDirectory.c_str(), strErrorInfo))
		return false;

	std::wstring sCurrDir;
	while (dirWalker.GetCurrent(sCurrDir))
	{
		// Add the current directory to the collection
		AddFSObjectToCollection(sCurrDir, true, fileInfoCollection);

		// Add all the files in the current directory to the collection
		std::vector<std::wstring> files, subdirectories;
		if (GetFiles(sCurrDir, files, false))
		{
			for (
				std::vector<std::wstring>::const_iterator iterFiles = files.begin();
				iterFiles != files.end();
				++iterFiles
				)
			{
				AddFSObjectToCollection(*iterFiles, false, fileInfoCollection);
			}
		}

		dirWalker.DoneWithCurrent();
	}

	return true;
}

/// <summary>
/// Returns a listing of all files/directories under System32\AppLocker.
/// </summary>
/// <param name="fileInfoCollection">Output: the collection object to populate with file/directory information</param>
/// <returns></returns>
bool AppLocker_EmergencyClean::ListAppLockerBinaryFiles(FileInfoCollection_t& fileInfoCollection)
{
	fileInfoCollection.clear();
	const std::wstring sRootDir = WindowsDirectories::System32Directory() + L"\\AppLocker";
	return RecursiveFileList(sRootDir, fileInfoCollection);
}

/// <summary>
/// Delete all files and directories under System32\AppLocker.
/// </summary>
/// <param name="sErrorInfo">Output: error info if any failures</param>
/// <returns>true if successful, false otherwise</returns>
bool AppLocker_EmergencyClean::DeleteAppLockerBinaryFiles(std::wstring& sErrorInfo)
{
	sErrorInfo.clear();
	std::wstringstream strErrorInfo;
	bool bSuccess = false;
	FileInfoCollection_t fileInfoCollection;
	std::set<std::wstring> directories;
	const std::wstring sRootDir = WindowsDirectories::System32Directory() + L"\\AppLocker";
	if (RecursiveFileList(sRootDir, fileInfoCollection))
	{
		strErrorInfo << L"Could not delete the following:" << std::endl;
		bSuccess = true;
		// Delete all the files, and build a sorted list of the directory names
		for (size_t ixFileInfo = 0; ixFileInfo < fileInfoCollection.size(); ++ixFileInfo)
		{
			FileInfo_t& fileInfo = fileInfoCollection[ixFileInfo];
			if (!fileInfo.bIsDirectory)
			{
				if (!DeleteFileW(fileInfo.sFullPath.c_str()))
				{
					bSuccess = false;
					DWORD dwLastErr = GetLastError();
					strErrorInfo << fileInfo.sFullPath << L": " << SysErrorMessage(dwLastErr) << std::endl;
				}
			}
			else
			{
				// Don't want to delete the root directory
				if (sRootDir != fileInfo.sFullPath)
				{
					directories.insert(fileInfo.sFullPath);
				}
			}
		}
		// Delete them in reverse alpha order (which should delete a directory's subdirectories before the parent
		for (
			std::set<std::wstring>::const_reverse_iterator rIterDir = directories.rbegin();
			rIterDir != directories.rend();
			rIterDir++)
		{
			if (!RemoveDirectoryW(rIterDir->c_str()))
			{
				bSuccess = false;
				DWORD dwLastErr = GetLastError();
				strErrorInfo << *rIterDir << L": " << SysErrorMessage(dwLastErr) << std::endl;
			}
		}
		if (!bSuccess)
		{
			sErrorInfo = strErrorInfo.str();
		}
	}
	return bSuccess;
}
