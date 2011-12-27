#include "stdafx.h"
#include "setconfig.h"
#include <vector>
#include <unordered_map>
#include <conio.h>

#pragma comment(lib, "XmlLite.lib")
#pragma comment(lib, "shlwapi.lib")

std::vector<FileInfo> Files;
std::unordered_map<std::wstring, std::wstring> Variables;

HRESULT ParseConfigurationFile(const wchar_t* filename);

void ParseLocalVarAttributes(IXmlReader* in, std::wstring& name, std::wstring& value)
{
	const wchar_t *localName;
	const wchar_t *data;
	HRESULT hr;
	while (S_OK == (hr = in->MoveToNextAttribute()))
	{
		in->GetLocalName(&localName, NULL);
		if (_wcsicmp(localName, L"name") == 0)
		{
			in->GetValue(&data, NULL);
			name = data;
		}
		else if (_wcsicmp(localName, L"value") == 0)
		{
			in->GetValue(&data, NULL);
			value = data;
		};
	};
};

void ParseFileAttributes(IXmlReader* in, FileInfo& link)
{
	const wchar_t *localName;
	const wchar_t *data;
	HRESULT hr;
	while (S_OK == (hr = in->MoveToNextAttribute()))
	{
		in->GetLocalName(&localName, NULL);
		if (_wcsicmp(localName, L"source") == 0)
		{
			in->GetValue(&data, NULL);
			link.Source = data;
		}
		else if (_wcsicmp(localName, L"target") == 0)
		{
			in->GetValue(&data,NULL);
			link.Target = data;
		}
		else if (_wcsicmp(localName, L"action") == 0)
		{
			in->GetValue(&data,NULL);
			link.Action = data;
		}
	};
};

void ParseIncludeAttributes(IXmlReader* in)
{
	const wchar_t *localName;
	const wchar_t *data;
	HRESULT hr;
	while (S_OK == (hr = in->MoveToNextAttribute()))
	{
		in->GetLocalName(&localName, NULL);
		if (_wcsicmp(localName, L"file") == 0)
		{
			in->GetValue(&data, NULL);
			if (FAILED(ParseConfigurationFile(data)))
			{
				printf("Unable to open '%S'.", data);
			}
		}
	};
};

void ParseConfigurationElements(IXmlReader* in)
{
	XmlNodeType nodeType;
	while (S_OK == in->Read(&nodeType))
	{
		switch (nodeType)
		{
		case XmlNodeType_Element:
			{
				const wchar_t *localName = 0;
				in->GetLocalName(&localName,NULL);
				if (_wcsicmp(localName, L"localvar") == 0)
				{
					std::wstring name, value;
					ParseLocalVarAttributes(in, name, value);
					
					auto it = Variables.find(name);
					if (it != Variables.end())
					{
						it->second = value;
					}
					else
					{
						Variables.insert(std::make_pair(name, value));
					}
				}
				else if (_wcsicmp(localName, L"file") == 0)
				{
					FileInfo link;
					link.Action = L"$DefaultAction";
					ParseFileAttributes(in, link);
					Files.push_back(link);
				}
				else if (_wcsicmp(localName, L"include") == 0)
				{
					ParseIncludeAttributes(in);
				}
				break;
			} 
		case XmlNodeType_Text:
		case XmlNodeType_EndElement:
			break;
		}
	}
};

HRESULT ParseConfigurationFile(const wchar_t* filename)
{
	HRESULT hr;

	IStream* stream_xml;
	if (FAILED(hr = SHCreateStreamOnFileW(filename,STGM_READ,&stream_xml)))
	{
		return hr;
	}

	IXmlReader* xread;
	if (FAILED(hr == CreateXmlReader(__uuidof(IXmlReader),(void **)&xread,NULL)))
	{
		stream_xml->Release();
		return hr;
	}

	xread->SetInput(stream_xml);
	xread->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
	ParseConfigurationElements(xread);
	xread->Release();
	stream_xml->Release();

	return S_OK;
};

int DeleteTargetFile(const wchar_t* target)
{
	if (!DeleteFileW(target))
	{
		int error = GetLastError();
		if (error != ERROR_FILE_NOT_FOUND)
		{
			printf("Error deleting existing file at '%S', error is %08.8lx\n", target, GetLastError());
			return -1;
		}
	};
	return 0;
}

int ProcessFiles()
{
	HMODULE kernel32 = LoadLibrary(L"kernel32.dll");
	typedef BOOLEAN (WINAPI* CREATE_SYMBOLIC_LINK_W)(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);
	CREATE_SYMBOLIC_LINK_W pfCreateSymbolicLinkW = NULL;
	if (kernel32 != NULL)
	{
		pfCreateSymbolicLinkW = (CREATE_SYMBOLIC_LINK_W) GetProcAddress(kernel32, "CreateSymbolicLinkW");
	};

	const wchar_t* override_action;
	auto override_action_it = Variables.find(L"$OverrideAction");
	if (override_action_it != Variables.end()) override_action = override_action_it->second.c_str();
	else override_action = nullptr;

	for (auto file_it = Files.begin(); file_it != Files.end(); ++file_it)
	{
		for (auto variable_it = Variables.begin(); variable_it != Variables.end(); ++variable_it)
		{
			str_replace(variable_it->first, variable_it->second, file_it->Source);
			str_replace(variable_it->first, variable_it->second, file_it->Target);
			str_replace(variable_it->first, variable_it->second, file_it->Action);
		}

		const wchar_t* action;
		if (override_action) action = override_action;
		else action = file_it->Action.c_str();

		const wchar_t* target = file_it->Target.c_str();
		const wchar_t* source = file_it->Source.c_str();

		if (pfCreateSymbolicLinkW && _wcsicmp(action, L"link") == 0)
		{
			if (DeleteTargetFile(target)) return -1;
			if (!pfCreateSymbolicLinkW(target, source, 0))
			{
				printf("Error creating symbolic link at '%S', error is %08.8lx; falling back to file copy\n", target, GetLastError());
				if (!CopyFileW(source, target, TRUE))
				{
					printf("Error copying file at '%S', error is %08.8lx\n", target, GetLastError());
					return -1;
				}
			}
		}
		else if (_wcsicmp(action, L"copy") == 0)
		{
			if (DeleteTargetFile(target)) return -1;
			if (!CopyFileW(file_it->Source.c_str(), file_it->Target.c_str(), TRUE))
			{
				printf("Error copying file at '%S', error is %08.8lx\n", file_it->Target.c_str(), GetLastError());
				return -1;
			};
		}
		else if (_wcsicmp(action, L"delete") == 0)
		{
			if (DeleteTargetFile(target)) return -1;
		}
		else
		{
			printf("Unknown action for file at '%S'\n", file_it->Target.c_str());
			return -1;
		}
	}

	FreeLibrary(kernel32);
	return 0;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		printf("setconfig - hook in a set of binary files via symbolic link from an xml config file\n");
		printf("usage:\nsetconfig configfile \nconfigfile is the input XML file\n");
		exit(1);
	}

	Variables.insert(std::make_pair(L"$DefaultAction", L"link"));

	bool error = false;
	HRESULT res = ParseConfigurationFile(L"user.xml");
	if (FAILED(res) && res != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	{
		printf("Unable to open 'user.xml'.\n");
		error = true;
	}

	for (int i = 1; i < argc; ++i)
	{
		if (FAILED(ParseConfigurationFile(argv[i])))
		{
			printf("Unable to open '%S'.\n", argv[i]);
			error = true;
		}
	}

	for (auto variable_it = Variables.begin(); variable_it != Variables.end(); ++variable_it)
	{
		for (auto replace_it = Variables.begin(); replace_it != Variables.end(); ++replace_it)
		{
			if (replace_it == variable_it) continue;
			str_replace(variable_it->first, variable_it->second, replace_it->second);
		}
	}

	if (ProcessFiles())
	{
		error = true;
	}
	
	if (error)
	{
		printf("\nPress any key to continue...");
#pragma warning(suppress: 6031)
		_getch();
	}

	return 0;
}