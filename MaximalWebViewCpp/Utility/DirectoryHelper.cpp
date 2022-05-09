#include "DirectoryHelper.h"
#include <Windows.h>

std::string DirectoryHelper::GetExecutableDirectoryA()
{
	if (executableDirectoryA != "")
		return executableDirectoryA;

	char szExecutablePath[MAX_PATH];
	GetModuleFileNameA(NULL, szExecutablePath, MAX_PATH);

	std::string executablePath(szExecutablePath);

	executableDirectoryA = executablePath.substr(0, executablePath.find_last_of("/\\")) + "/";
	executableDirectoryA = NormalizePathA(executableDirectoryA); // Replace \\ with /
	return executableDirectoryA;
}

std::wstring DirectoryHelper::GetExecutableDirectory()
{
	if (executableDirectory != L"")
		return executableDirectory;

	TCHAR szExecutablePath[MAX_PATH];
	GetModuleFileName(NULL, szExecutablePath, MAX_PATH);

	std::wstring executablePath(szExecutablePath);

	executableDirectory = executablePath.substr(0, executablePath.find_last_of(L"/\\")) + L"/";
	executableDirectory = NormalizePath(executableDirectory); // Replace \\ with /
	return executableDirectory;
}

std::string DirectoryHelper::NormalizePathA(std::string path)
{
	if (!path.empty())
	{
		for (size_t i = 0; i < path.length(); ++i)
		{
			if (path[i] == '\\')
				path[i] = '/';
		}
	}
	return path;
}

std::wstring DirectoryHelper::NormalizePath(std::wstring path)
{
	if (!path.empty())
	{
		for (size_t i = 0; i < path.length(); ++i)
		{
			if (path[i] == L'\\')
				path[i] = L'/';
		}
	}
	return path;
}

std::string DirectoryHelper::executableDirectoryA = "";
std::wstring DirectoryHelper::executableDirectory = L"";
