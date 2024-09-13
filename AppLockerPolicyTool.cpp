// AppLockerPolicyTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

/*
Command-line tool to manage AppLocker policy on the local machine.
Reads, writes, or deletes AppLocker policy via the local GPO or CSP/MDM interfaces.
CSP configured locally (without an MDM server) using the WMI bridge.
Also provides an emergency interface directly into the AppLocker policy cache in the file system.
*/

#include <Windows.h>
#include <iostream>
#include <iomanip>
#include "AppLockerPolicy.h"
#include "Utf8FileUtility.h"
#include "FileSystemUtils.h"
#include "StringUtils.h"
#include "WhoAmI.h"

/// <summary>
/// Write command-line syntax to stderr and then exit.
/// </summary>
/// <param name="szError">Caller-supplied error text</param>
/// <param name="argv0">The program's argv[0] value</param>
void Usage(const wchar_t* szError, const wchar_t* argv0)
{
	std::wstring sExe = GetFileNameFromFilePath(argv0);
	if (szError)
		std::wcerr << szError << std::endl;
	std::wcerr
		<< std::endl
		<< L"Usage:" << std::endl
		<< std::endl
		<< L"  Configuration Service Provider (CSP) operations:" << std::endl
		<< std::endl
		<< L"    " << sExe << L" -csp -get [-out filename]" << std::endl
		<< L"    " << sExe << L" -csp -set filename [-gn groupname]" << std::endl
		<< L"    " << sExe << L" -csp -deleteall" << std::endl
		<< std::endl
		<< L"  Local Group Policy Object (LGPO) operations:" << std::endl
		<< std::endl
		<< L"    " << sExe << L" -lgpo -get [-out filename]" << std::endl
		<< L"    " << sExe << L" -lgpo -set filename" << std::endl
		<< L"    " << sExe << L" -lgpo -clear" << std::endl
		<< std::endl
		<< L"  Effective Group Policy Object (GPO) operations:" << std::endl
		<< std::endl
		<< L"    " << sExe << L" -gpo -get [-out filename]" << std::endl
		<< std::endl
		<< L"  Last resort emergency operations:" << std::endl
		<< std::endl
		<< L"    " << sExe << L" -911 [-list | -deleteall]" << std::endl
		<< std::endl;
	exit(-1);
}

// Forward declare the little helper functions called by wmain.
int GetLgpoPolicy(const std::wstring& sOutputFile);
int GetGpoEffectivePolicy(const std::wstring& sOutputFile);
int SetLgpoPolicy(const std::wstring& sFilename);
int ClearLgpoPolicy();
int GetCspPolicies(const std::wstring& sOutputFile);
int SetCspPolicy(const std::wstring& sFilename, const std::wstring& sGroupName);
int DeleteAllCspPolicies();
int Do911List();
int Do911DeleteAll();

//TODO: add ability to retrieve timestamp info (by itself) from policy.

int wmain(int argc, wchar_t** argv)
{
	bool bCspMode = false, bLgpoMode = false, bGpoEffectiveMode = false, b911Mode = false;
	bool bGetPolicies = false, bOutToFile = false, bSetPolicies = false, bDeleteAll = false, bClear = false, bList = false;
	std::wstring sPolicyFile, sOutputFile;
	bool bGroupName = false;
	std::wstring sGroupName;

	// Parse command line options
	int ixArg = 1;
	while (ixArg < argc)
	{
		if (0 == _wcsicmp(L"-csp", argv[ixArg]))
		{
			bCspMode = true;
		}
		else if (0 == _wcsicmp(L"-lgpo", argv[ixArg]))
		{
			bLgpoMode = true;
		}
		else if (0 == _wcsicmp(L"-gpo", argv[ixArg]))
		{
			bGpoEffectiveMode = true;
		}
		else if (0 == _wcsicmp(L"-911", argv[ixArg]))
		{
			b911Mode = true;
		}
		else if (0 == _wcsicmp(L"-get", argv[ixArg]))
		{
			bGetPolicies = true;
		}
		else if (0 == _wcsicmp(L"-out", argv[ixArg]))
		{
			bOutToFile = true;
			if (++ixArg >= argc)
				Usage(L"Missing arg for -out", argv[0]);
			sOutputFile = argv[ixArg];
		}
		else if (0 == _wcsicmp(L"-set", argv[ixArg]))
		{
			bSetPolicies = true;
			if (++ixArg >= argc)
				Usage(L"Missing arg for -set", argv[0]);
			sPolicyFile = argv[ixArg];
		}
		else if (0 == _wcsicmp(L"-deleteall", argv[ixArg]))
		{
			bDeleteAll = true;
		}
		else if (0 == _wcsicmp(L"-list", argv[ixArg]))
		{
			bList = true;
		}
		else if (0 == _wcsicmp(L"-clear", argv[ixArg]))
		{
			bClear = true;
		}
		else if (0 == _wcsicmp(L"-gn", argv[ixArg]))
		{
			bGroupName = true;
			if (++ixArg >= argc)
				Usage(L"Missing arg for -gn", argv[0]);
			sGroupName = argv[ixArg];
		}
		else
		{
			std::wcerr << L"Unrecognized command-line option: " << argv[ixArg] << std::endl;
			Usage(NULL, argv[0]);
		}
		++ixArg;
	}

	// Some basic validation:
	size_t nModeCount = 0, nOperationCount = 0;
	// Count modes
	if (bCspMode) nModeCount++;
	if (bLgpoMode) nModeCount++;
	if (bGpoEffectiveMode) nModeCount++;
	if (b911Mode) nModeCount++;
	// Count operations
	if (bGetPolicies) nOperationCount++;
	if (bSetPolicies) nOperationCount++;
	if (bDeleteAll) nOperationCount++;
	if (bClear) nOperationCount++;
	if (bList) nOperationCount++;
	if (1 != nModeCount || 1 != nOperationCount)
	{
		Usage(L"Need to specify one policy mode (CSP, LGPO, GPO, or 911) and one operation.", argv[0]);
	}
	// Check some invalid combinations
	if (
		(bGroupName && !(bCspMode && bSetPolicies)) || // group name valid only when setting CSP/MDM policies
		(bOutToFile && !bGetPolicies)               || // output file only for get-policy operation
		(bGpoEffectiveMode && !bGetPolicies)           // -gpo must be used with -get
		)
	{ 
		Usage(L"Unsupported mode/operation combination.", argv[0]);
	}

	if (bLgpoMode)
	{
		if (bGetPolicies)
		{
			return GetLgpoPolicy(sOutputFile);
		}
		if (bSetPolicies)
		{
			return SetLgpoPolicy(sPolicyFile);
		}
		if (bClear)
		{
			return ClearLgpoPolicy();
		}
	}
	else if (bGpoEffectiveMode)
	{
		if (bGetPolicies)
		{
			return GetGpoEffectivePolicy(sOutputFile);
		}
	}
	else if (bCspMode)
	{
		// AppLocker CSP interfaces require running as System. They fail silently even if you run with admin rights but not as System.
		// No "access denied" errors identifiable at all.
		// So, proactively check for System and error out if not running as System.
		WhoAmI whoAmI;
		if (!whoAmI.IsSystem())
		{
			std::wcerr
				<< L"Error: AppLocker CSP interfaces are accessible only to the Local System account." << std::endl
				<< L"Currently running as "
				<< whoAmI.GetUserCSid().toDomainAndUsername(true) << std::endl;
			return -3;
		}
		if (bGetPolicies)
		{
			return GetCspPolicies(sOutputFile);
		}
		if (bSetPolicies)
		{
			return SetCspPolicy(sPolicyFile, sGroupName);
		}
		if (bDeleteAll)
		{
			return DeleteAllCspPolicies();
		}
	}
	else if (b911Mode)
	{
		if (bList)
		{
			return Do911List();
		}
		if (bDeleteAll)
		{
			return Do911DeleteAll();
		}
	}

	Usage(L"Unsupported mode/operation combination.", argv[0]);
}

// ------------------------------------------------------------------------------------------

/// <summary>
/// Class that wraps writing UTF-8 to an fstream or to stdout
/// </summary>
class wostreamWrapper
{
public:
	/// <summary>
	/// Constructor. Pass in a file name to write to the filename, or an empty string to write to stdout.
	/// </summary>
	/// <param name="sFilename">Filename to write to (empty string for stdout)</param>
	wostreamWrapper(const std::wstring& sFilename)
		: m_pStream(&std::wcout), m_bUsingFilestream(false)
	{
		if (sFilename.length() > 0)
		{
			m_FileStream.open(sFilename, std::ios_base::out);
			if (!m_FileStream.fail())
			{
				m_pStream = &m_FileStream;
				m_bUsingFilestream = true;
			}
		}
		std::locale loc = Utf8FileUtility::LocaleForWritingUtf8File();
		m_pStream->imbue(loc);
	}
	~wostreamWrapper()
	{
		// Close filestream if opened.
		if (m_bUsingFilestream)
			m_FileStream.close();
	}
	std::wostream& stream()
	{
		return *m_pStream;
	}
private:
	std::wostream* m_pStream;
	std::wofstream m_FileStream;
	bool m_bUsingFilestream;
private:
	// Not implemented
	wostreamWrapper(const wostreamWrapper&) = delete;
	wostreamWrapper& operator = (const wostreamWrapper&) = delete;
};

// ------------------------------------------------------------------------------------------

int GetLgpoPolicy(const std::wstring& sOutputFile)
{
	std::wstring sAppLockerPolicyXml, sErrorInfo;
	if (AppLockerPolicy_LGPO::GetLocalPolicy(sAppLockerPolicyXml, sErrorInfo))
	{
		wostreamWrapper os(sOutputFile);
		os.stream() << sAppLockerPolicyXml << std::endl;
		return 0;
	}
	else
	{
		std::wcout << L"Failed to get AppLocker LGPO policy: " << sErrorInfo << std::endl;
		return -2;
	}
}

int GetGpoEffectivePolicy(const std::wstring& sOutputFile)
{
	std::wstring sAppLockerPolicyXml, sErrorInfo;
	if (AppLockerPolicy_LGPO::GetEffectivePolicy(sAppLockerPolicyXml, sErrorInfo))
	{
		wostreamWrapper os(sOutputFile);
		os.stream() << sAppLockerPolicyXml << std::endl;
		return 0;
	}
	else
	{
		std::wcout << L"Failed to get AppLocker effective GPO policy: " << sErrorInfo << std::endl;
		return -2;
	}
}


int SetLgpoPolicy(const std::wstring& sFilename)
{
	std::wstring sErrorInfo;
	if (AppLockerPolicy_LGPO::SetPolicyFromFile(sFilename, sErrorInfo))
	{
		std::wcout << L"LGPO policy set." << std::endl;
		return 0;
	}
	else
	{
		std::wcout << L"Failed to set AppLocker LGPO policy: " << sErrorInfo << std::endl;
		return -2;
	}
}

int ClearLgpoPolicy()
{
	std::wstring sErrorInfo;
	if (AppLockerPolicy_LGPO::ClearPolicy(sErrorInfo))
	{
		std::wcout << L"LGPO policy cleared." << std::endl;
		return 0;
	}
	else
	{
		std::wcout << L"Failed to clear AppLocker LGPO policy: " << sErrorInfo << std::endl;
		return -2;
	}
}

bool CspStatusCheck(const AppLockerPolicy_CSP& csp)
{
	std::wstring sErrorInfo;
	bool retval = csp.StatusOK(&sErrorInfo);
	if (!retval)
		std::wcout << L"CSP interface initialization failed: " << sErrorInfo << std::endl;
	return retval;
}

int GetCspPolicies(const std::wstring& sOutputFile)
{
	AppLockerPolicies_t policies;
	AppLockerPolicy_CSP csp;
	if (!CspStatusCheck(csp))
	{
		return -1;
	}

	if (!csp.GetPolicies(policies))
	{
		std::wcout << L"AppLockerPolicy_CSP Get failed." << std::endl;
		return -2;
	}

	// If there are multiple policies defined through CSP, write them each to file, preceded by policy name.
	// If there's just one, just write it to the file without labeling.
	//TODO: Output needs to be something more programmatically consumable when there's more than one CSP-configured policy. JSON and a different exit code? Something with more obvious delimiter characters?
	wostreamWrapper os(sOutputFile);
	for (
		AppLockerPolicies_t::const_iterator iterPolicies = policies.begin();
		iterPolicies != policies.end();
		++iterPolicies
		)
	{
		if (policies.size() > 1)
		{
			os.stream()
				<< std::endl
				<< L"Policy name: " << iterPolicies->first << std::endl
				<< std::endl;
		}
		os.stream() << iterPolicies->second.Policy() << std::endl;
	}

	return 0;
}

int SetCspPolicy(const std::wstring& sFilename, const std::wstring& sGroupName)
{
	AppLockerPolicies_t policies;
	AppLockerPolicy_CSP csp;
	if (!CspStatusCheck(csp))
	{
		return -1;
	}

	bool ret;
	std::wstring sErrorInfo;
	if (sGroupName.length() > 0)
	{
		ret = csp.SetPolicyFromFile(sFilename, sGroupName, sErrorInfo);
	}
	else
	{
		ret = csp.SetPolicyFromFile(sFilename, sErrorInfo);
	}
	if (ret)
	{
		std::wcout << L"Policy set." << std::endl;
		std::wcout << sErrorInfo << std::endl;
		return 0;
	}
	else
	{
		std::wcout << L"Policy not set: " << sErrorInfo << std::endl;
		return -2;
	}
}

int DeleteAllCspPolicies()
{
	AppLockerPolicies_t policies;
	AppLockerPolicy_CSP csp;
	if (!CspStatusCheck(csp))
	{
		return -1;
	}

	bool bPoliciesDeleted = false;
	std::wstring sErrorInfo;
	csp.DeleteAllPolicies(bPoliciesDeleted, sErrorInfo);
	if (bPoliciesDeleted)
	{
		std::wcout << L"CSP AppLocker policies deleted." << std::endl;
	}
	else
	{
		std::wcout << L"No CSP AppLocker policies deleted." << std::endl;
	}
	if (0 == sErrorInfo.length())
		std::wcout << L"No errors detected." << std::endl;
	else
		std::wcout << sErrorInfo << std::endl;

	return 0;
}

int Do911List()
{
	/*
File creation time   File last written    Filesize  File path
2021-01-07 06:08:20  2021-01-07 06:08:20      8192
	*/
	std::wcout << L"File creation time   File last written    Filesize  File path" << std::endl;
	FileInfoCollection_t fileInfoCollection;
	if (AppLocker_EmergencyClean::ListAppLockerBinaryFiles(fileInfoCollection))
	{
		for (
			FileInfoCollection_t::const_iterator iterFI = fileInfoCollection.begin();
			iterFI != fileInfoCollection.end();
			++iterFI
			)
		{
			if (!iterFI->bIsDirectory)
			{
				std::wcout << iterFI->sCreateTime << L"  " << iterFI->sLastWriteTime << L"  " << std::setw(8) << iterFI->filesize.QuadPart << L"  " << iterFI->sFullPath << std::endl;
			}
			else
			{
				std::wcout << iterFI->sCreateTime << L"  " << iterFI->sLastWriteTime << L"  " << std::setw(8) << L"" << L"  " << iterFI->sFullPath << std::endl;
			}
		}
	}
	return 0;
}

int Do911DeleteAll()
{
	if (AppLocker_EmergencyClean::DeleteAppLockerBinaryFiles())
	{
		std::wcout << L"AppLocker binary files deleted." << std::endl;
		return 0;
	}
	else
	{
		std::wcout << L"Failure: AppLocker binary file deletion failed." << std::endl;
		std::wcout << std::endl;
		Do911List();
		return -1;
	}
}