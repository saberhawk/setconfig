// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#pragma once

// Windows XP SP2 API
//#include "sdkddkver.h"
#define NTDDI_VERSION NTDDI_WIN7
#define WIN32_LEAN_AND_MEAN	

#include <stdio.h>
#include <ctype.h>
#include <wchar.h>
#include <windows.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <xmllite.h> // Provided in "Windows Software Development Kit (SDK) for Windows Vista"
#include <string>

using namespace std;