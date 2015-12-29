// Unicode support by Jim Park -- 08/23/2007

#include <windows.h>
#include "../../ExDll/nsis_tchar.h"

#define STR_SIZE 1024

void RegFile(TCHAR cmd, TCHAR *file, int x64);
void RegDll(TCHAR *file);
void RegTypeLib(TCHAR *file);
void DeleteFileOnReboot(TCHAR *pszFile);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
  TCHAR *cmdline;
  TCHAR seekchar = _T(' ');
  TCHAR *keyname, *file;

  // These are turned into heap memory to avoid _chkstk
  keyname = (TCHAR*) GlobalAlloc(GPTR, STR_SIZE*sizeof(TCHAR));
  file    = (TCHAR*) GlobalAlloc(GPTR, STR_SIZE*sizeof(TCHAR));

  cmdline = GetCommandLine();
  if (*cmdline == _T('\"'))
    seekchar = *cmdline++;

  while (*cmdline && *cmdline != seekchar)
    cmdline = CharNext(cmdline);
  cmdline = CharNext(cmdline);
  while (*cmdline == _T(' '))
    cmdline++;

  if (*cmdline++ != _T('/'))
  {
    ExitProcess(1);
    return 0;
  }

  if (*cmdline == _T('S'))
  {
    HKEY rootkey;

    if (SUCCEEDED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\NSIS.Library.RegTool.v3"), 0, KEY_READ, &rootkey)))
    {
      while (RegEnumKey(rootkey, 0, keyname, STR_SIZE) == ERROR_SUCCESS)
      {
        HKEY key;

        if (SUCCEEDED(RegOpenKeyEx(rootkey, keyname, 0, KEY_READ, &key)))
        {
          DWORD t, count, l = sizeof(DWORD);

          if (SUCCEEDED(RegQueryValueEx(key, _T("count"), NULL, &t, (LPBYTE) &count, &l)) && t == REG_DWORD)
          {
            DWORD j;
            TCHAR valname[128], mode[3];

            for (j = 1; j <= count; j++)
            {
              wsprintf(valname, _T("%u.mode"), j);
              l = sizeof(mode);
              if (FAILED(RegQueryValueEx(key, valname, NULL, &t, (LPBYTE) mode, &l)) || t != REG_SZ)
                continue;

              wsprintf(valname, _T("%u.file"), j);
              l = STR_SIZE*sizeof(TCHAR);
              if (FAILED(RegQueryValueEx(key, valname, NULL, &t, (LPBYTE) file, &l)) || t != REG_SZ)
                continue;

              file[STR_SIZE - 1] = 0;

              // JP: Note, if this mode[1] is used as anything but a boolean later on,
              // we'll need to consider the next line carefully.
              RegFile(mode[0], file, mode[1] == 'X');
            }
          }

          RegCloseKey(key);
          RegDeleteKey(rootkey, keyname);
        }
      }

      RegCloseKey(rootkey);
      RegDeleteKey(HKEY_LOCAL_MACHINE, _T("Software\\NSIS.Library.RegTool.v3"));
    }

    {
      if (GetModuleFileName(GetModuleHandle(NULL), file, STR_SIZE))
      {
        DeleteFileOnReboot(file);
      }
    }
  }
  else
  {
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    OleInitialize(NULL);

    if (*cmdline == _T('D'))
    {
      RegDll(cmdline + 1);
    }
    else if (*cmdline == _T('T'))
    {
      RegTypeLib(cmdline + 1);
    }

    OleUninitialize();
    SetErrorMode(0);
  }

  GlobalFree(keyname);
  GlobalFree(file);
  ExitProcess(0);
  return 0;
}

void SafeWow64EnableWow64FsRedirection(BOOL Wow64FsEnableRedirection)
{
  HMODULE kernel = GetModuleHandle(_T("kernel32"));
  if (kernel)
  {
    FARPROC proc = GetProcAddress(kernel, "Wow64EnableWow64FsRedirection");
    if (proc)
    {
      typedef BOOL (WINAPI *Wow64EnableWow64FsRedirectionPtr)(BOOL);
      Wow64EnableWow64FsRedirectionPtr Wow64EnableWow64FsRedirectionFunc =
        (Wow64EnableWow64FsRedirectionPtr) proc;

      Wow64EnableWow64FsRedirectionFunc(Wow64FsEnableRedirection);
    }
  }
}

void RegFile(TCHAR cmd, TCHAR *file, int x64)
{
  TCHAR* self = (TCHAR*) GlobalAlloc(GPTR, sizeof(TCHAR)*STR_SIZE);
  TCHAR* cmdline = (TCHAR*) GlobalAlloc(GPTR, sizeof(TCHAR)*STR_SIZE);

  int ready = 0;

  if (!*file || (cmd != _T('D') && cmd != _T('T') && cmd != _T('E')))
    return;

  if (cmd == _T('E'))
  {
    wsprintf(cmdline, _T("\"%s\" /regserver"), file);
    ready++;
  }
  else if (!x64)
  {
    if (GetModuleFileName(GetModuleHandle(NULL), self, STR_SIZE))
    {
      wsprintf(cmdline, _T("\"%s\" /%c%s"), self, cmd, file);
      ready++;
    }
  }
  else
  {
    if (GetSystemDirectory(self, STR_SIZE))
    {
      wsprintf(cmdline, _T("\"%s\\regsvr32.exe\" /s \"%s\""), self, file);
      ready++;

      SafeWow64EnableWow64FsRedirection(FALSE);
    }
  }

  if (ready)
  {
    PROCESS_INFORMATION pi;
    STARTUPINFO si = { sizeof(STARTUPINFO) };

    if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
      CloseHandle(pi.hThread);

      WaitForSingleObject(pi.hProcess, INFINITE);

      CloseHandle(pi.hProcess);
    }
    
    if (x64)
    {
      SafeWow64EnableWow64FsRedirection(TRUE);
    }
  }

  GlobalFree(self);
  GlobalFree(cmdline);
}

void RegDll(TCHAR *file)
{
  HMODULE mod = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (mod)
  {
    FARPROC regfunc = GetProcAddress(mod, "DllRegisterServer");
    if (regfunc)
      regfunc();
    FreeLibrary(mod);
  }
}

void RegTypeLib(TCHAR *file)
{
#ifdef _UNICODE
 ITypeLib* tlib;
 if (SUCCEEDED(LoadTypeLib(file, &tlib))) {
   RegisterTypeLib(tlib, file, NULL);
   tlib->lpVtbl->Release(tlib);
 }
#else
  WCHAR wfile[STR_SIZE];

  if (MultiByteToWideChar(CP_ACP, 0, file, -1, wfile, STR_SIZE) != 0)
  {
    ITypeLib* tlib;
    if (SUCCEEDED(LoadTypeLib(wfile, &tlib))) {
      RegisterTypeLib(tlib, wfile, NULL);
      tlib->lpVtbl->Release(tlib);
    }
  }
#endif
}

char *mystrstriA(char *a, const char *b)
{
  int l = lstrlenA(b);
  while (lstrlenA(a) >= l)
  {
    char c = a[l];
    a[l] = 0;
    if (!lstrcmpiA(a, b))
    {
      a[l] = c;
      return a;
    }
    a[l] = c;
    a = CharNextA(a);
  }
  return NULL;
}

void mini_memcpy(void *out, const void *in, int len)
{
  char *c_out=(char*)out;
  char *c_in=(char *)in;
  while (len-- > 0)
  {
    *c_out++=*c_in++;
  }
}

HANDLE myOpenFile(const TCHAR *fn, DWORD da, DWORD cd)
{
  int attr = GetFileAttributes(fn);
  return CreateFile(
    fn,
    da,
    FILE_SHARE_READ,
    NULL,
    cd,
    attr == INVALID_FILE_ATTRIBUTES ? 0 : attr,
    NULL
  );
}

/** Modifies the wininit.ini file to rename / delete a file.
 *
 * @param prevName The previous / current name of the file.
 * @param newName The new name to move the file to.  If NULL, the current file
 * will be deleted.
 */
void RenameViaWininit(TCHAR* prevName, TCHAR* newName)
{
  static char szRenameLine[1024];
  static TCHAR wininit[1024];
  static TCHAR tmpbuf[1024];
#ifdef _UNICODE
  static char shortExisting[1024];
  static char shortNew[1024];
#endif

  int cchRenameLine;
  static const char szRenameSec[] = "[Rename]\r\n"; // rename section marker
  HANDLE hfile;
  DWORD dwFileSize;
  DWORD dwBytes;
  DWORD dwRenameLinePos;
  char *pszWinInit;   // Contains the file contents of wininit.ini

  int spn;   // length of the short path name in TCHARs.

  lstrcpy(tmpbuf, _T("NUL"));

  if (newName) {
    // create the file if it's not already there to prevent GetShortPathName from failing
    CloseHandle(myOpenFile(newName,0,CREATE_NEW));
    spn = GetShortPathName(newName,tmpbuf,1024);
    if (!spn || spn > 1024)
      return;
  }
  // wininit is used as a temporary here
  spn = GetShortPathName(prevName,wininit,1024);
  if (!spn || spn > 1024)
    return;

 // Because wininit.ini is an ASCII text file, we need to be careful what we
 // convert here to TCHARs.

#ifdef _UNICODE
  // The short name produced by GetShortPathName is always in the ASCII range
  // of characters.
 
  // Convert short name of new name to ANSI
  if (WideCharToMultiByte(CP_ACP, 0, tmpbuf, -1, shortNew, sizeof(shortNew), NULL, NULL) == 0)
  {
     // We have a failure in conversion to ANSI
     return;
  }

  // Convert short name of old name to ANSI
  if (WideCharToMultiByte(CP_ACP, 0, wininit, -1, shortExisting, sizeof(shortExisting), NULL, NULL) == 0)
  {
     // We have a failure in conversion to ANSI
     return;
  }

  cchRenameLine = wsprintfA(szRenameLine, "%s=%s\r\n", shortNew, shortExisting);
#else
  cchRenameLine = wsprintfA(szRenameLine, "%s=%s\r\n", tmpbuf, wininit);
#endif
  // Get the path to the wininit.ini file.
  GetWindowsDirectory(wininit, 1024-16);
  lstrcat(wininit, _T("\\wininit.ini"));

  hfile = myOpenFile(wininit, GENERIC_READ | GENERIC_WRITE, OPEN_ALWAYS);

  if (hfile != INVALID_HANDLE_VALUE)
  {
    // We are now working on the Windows wininit file
    dwFileSize = GetFileSize(hfile, NULL);
    pszWinInit = (char*) GlobalAlloc(GPTR, dwFileSize + cchRenameLine + 10);

    if (pszWinInit != NULL)
    {
      if (ReadFile(hfile, pszWinInit, dwFileSize, &dwBytes, NULL) && dwFileSize == dwBytes)
      {
        // Look for the rename section in the current file.
        LPSTR pszRenameSecInFile = mystrstriA(pszWinInit, szRenameSec);
        if (pszRenameSecInFile == NULL)
        {
          // No rename section.  So we add it to the end of file.
          lstrcpyA(pszWinInit+dwFileSize, szRenameSec);
          dwFileSize += 10;
          dwRenameLinePos = dwFileSize;
        }
        else
        {
          // There is a rename section, but is there another section after it?
          char *pszFirstRenameLine = pszRenameSecInFile+10;
          char *pszNextSec = mystrstriA(pszFirstRenameLine,"\n[");
          if (pszNextSec)
          {
            char *p = pszWinInit + dwFileSize;
            char *pEnd = pszWinInit + dwFileSize + cchRenameLine;

            while (p > pszNextSec)
            {
              *pEnd-- = *p--;
            }

            dwRenameLinePos = pszNextSec - pszWinInit + 1; // +1 for the \n
          }
          // rename section is last, stick item at end of file
          else dwRenameLinePos = dwFileSize;
        }

        mini_memcpy(&pszWinInit[dwRenameLinePos], szRenameLine, cchRenameLine);
        dwFileSize += cchRenameLine;

        SetFilePointer(hfile, 0, NULL, FILE_BEGIN);
        WriteFile(hfile, pszWinInit, dwFileSize, &dwBytes, NULL);

        GlobalFree(pszWinInit);
      }
    }
    
    CloseHandle(hfile);
  }
}

void DeleteFileOnReboot(TCHAR *pszFile)
{
  BOOL fOk = 0;
  HMODULE hLib=GetModuleHandle(_T("KERNEL32.dll"));
  if (hLib)
  {
#ifdef _UNICODE
    typedef BOOL (WINAPI *mfea_t)(LPCTSTR lpExistingFileName,LPCTSTR lpNewFileName,DWORD dwFlags);
    mfea_t mfea;
    mfea=(mfea_t) GetProcAddress(hLib,"MoveFileExW");
    if (mfea)
    {
      fOk=mfea(pszFile, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
#else
    typedef BOOL (WINAPI *mfea_t)(LPCSTR lpExistingFileName,LPCSTR lpNewFileName,DWORD dwFlags);
    mfea_t mfea;
    mfea=(mfea_t) GetProcAddress(hLib,"MoveFileExA");
    if (mfea)
    {
      fOk=mfea(pszFile, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
#endif
  }

  if (!fOk)
  {
    RenameViaWininit(pszFile, NULL);
  }
}
