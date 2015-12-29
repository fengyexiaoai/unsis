#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x0400

#include <windows.h>
#include <winnls.h>
#include <tchar.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

class CodePageTable
{
public:
   typedef std::map<tstring, UINT> Map;
   typedef std::vector<tstring>    StrVec;

   void Init();
   void AddLanguage(const TCHAR* name, LANGID langid);
   void AddCodePage(const TCHAR* name, UINT langid);
   UINT Find(const TCHAR* name) const;
   
   // data members
   Map    m_strToCP;
   StrVec m_keys;
};

void CodePageTable::AddLanguage(const TCHAR* name, LANGID langid)
{
   UINT codePage;
   GetLocaleInfo(MAKELCID(langid, SORT_DEFAULT), 
                 LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                 (LPTSTR)&codePage,
                 sizeof(codePage)/sizeof(TCHAR));
   this->m_strToCP[name] = codePage;
   this->m_keys.push_back(name);
}

void CodePageTable::AddCodePage(const TCHAR* name, UINT codePage)
{
   this->m_strToCP[name] = codePage;
   this->m_keys.push_back(name);
}
/*
CodePageTable cpTable[] =
{
   TEXT("ANSI"),                0,
   TEXT("Thai"),                874,
   TEXT("Japanese"),            932,
   TEXT("Simplified_Chinese"),  936,
   TEXT("Korean"),              949,
   TEXT("Traditional_Chinese"), 950,
   TEXT("Central_European"),    1250,
   TEXT("Cyrillic"),            1251,
   TEXT("Latin_I"),             1252,
   TEXT("Greek"),               1253,
   TEXT("Turkish"),             1254,
   TEXT("Hebrew"),              1255,
   TEXT("Arabic"),              1256,
   TEXT("Baltic"),              1257,
   TEXT("Vietnamese"),          1258,
   TEXT("UTF-8"),               65001,
   SUBLANG_ARABIC_IRAQ
};
*/
void CodePageTable::Init()
{
   // Add these first so that langs.h when updated will overwrite these.
   this->AddCodePage(_T("ANSI"), 0);
   this->AddCodePage(_T("CENTRAL_EUROPEAN"), 1250);
   this->AddCodePage(_T("CYRILLIC"), 1251);
   this->AddCodePage(_T("LATIN_I"), 1252);
   this->AddCodePage(_T("BALTIC"), 1257);
   this->AddCodePage(_T("UTF-8"), 65001);
   this->AddCodePage(_T("WELSH"), 1252);
   this->AddCodePage(_T("ZULU"), 1252);
   this->AddCodePage(_T("YAKUT"), 1251);
   this->AddCodePage(_T("SESOTHO"), 1252);
   this->AddCodePage(_T("SAMI"), 1252);
   this->AddCodePage(_T("ROMANSH"), 1252);
   this->AddCodePage(_T("QUECHUA"), 1252);
   this->AddCodePage(_T("MOHAWK"), 1252);
   this->AddCodePage(_T("MALTESE"), 1252);
   this->AddCodePage(_T("KINYARWANDA"), 1252);
   this->AddCodePage(_T("K'ICHE"), 1252);
   this->AddCodePage(_T("IRISH"), 1252);
   this->AddCodePage(_T("BOSNIAN"), 1251);
   this->AddCodePage(_T("BRETON"), 1252);
   this->AddCodePage(_T("KURDISH"), 1254);
   this->AddCodePage(_T("LUXEMBOURGISH"), 1252);
   this->AddCodePage(_T("VALENCIAN"), 1252);
   this->AddCodePage(_T("SPANISHINTERNATIONAL"), 1252);
   this->AddCodePage(_T("DEFAULT"), CP_ACP);

   // langs.h auto-generated by generate_langs.pl
#include "langs.h"

   // Synonyms
   this->AddCodePage(_T("NORWEGIANNYNORSK"), this->Find(_T("NORWEGIAN_NYNORSK")));
   this->AddCodePage(_T("PORTUGUESEBR"), this->Find(_T("PORTUGUESE_BRAZILIAN")));
   this->AddCodePage(_T("SERBIANLATIN"), this->Find(_T("SERBIAN_LATIN")));
   this->AddCodePage(_T("SIMPCHINESE"), this->Find(_T("CHINESE_SIMPLIFIED")));
   this->AddCodePage(_T("TRADCHINESE"), this->Find(_T("CHINESE_TRADITIONAL")));

   std::sort(this->m_keys.begin(), this->m_keys.end());
}

UINT CodePageTable::Find(const TCHAR* name) const
{
   Map::const_iterator iter = this->m_strToCP.find(name);
   UINT rval = -1;

   if (iter != this->m_strToCP.end())
      {
      rval = (*iter).second;
      }

   return rval;
}


int _tmain(int argc, _TCHAR* argv[])
{
   UINT lang = CP_ACP;
   char buf[1024];
   wchar_t bufw[1024];
   TCHAR* source;

   CodePageTable cpTable;
   cpTable.Init();

   if (argc < 2 || argc > 3)
      {
      ::_tprintf_s(TEXT("Usage: %s [Language] {text file}\n"), argv[0]);
      ::_putts(TEXT("\nSupported Languages are:"));
      
      CodePageTable::StrVec::const_iterator iter     = cpTable.m_keys.begin();
      CodePageTable::StrVec::const_iterator end_iter = cpTable.m_keys.end();
      while (iter != end_iter)
         {
         ::_fputts(TEXT("   "), stdout);
         ::_putts((*iter).c_str());
         ++iter;
         }
      return -1;
      }

   if (argc == 3)
      {
      _tcsupr_s(argv[1], _tcslen(argv[1])+1);
      lang = cpTable.Find(argv[1]);
      if (lang == -1)
         {
         ::_tprintf_s(_T("%s is not a valid language!\n"), argv[1]);
         return -1;
         }

      source = argv[2];
      }
   else
      {
      source = argv[1];
      }

   FILE* fp;
   _tfopen_s(&fp, source, TEXT("r"));

   TCHAR filename[1024];
   ::_tcscpy_s(filename, source);
   ::_tcscat_s(filename, TEXT(".u16"));

   FILE* fpw;
   _tfopen_s(&fpw, filename, TEXT("w, ccs=UNICODE"));

   int line = 0;

   // We are explicitly reading it as an ASCII file.
   while (::fgets(buf, sizeof(buf), fp) != NULL && fpw)
      {
      ++line;
      if (::MultiByteToWideChar(lang, MB_PRECOMPOSED, buf, sizeof(buf), bufw, sizeof(bufw)/sizeof(bufw[0])) == 0)
         {
         ::_tprintf(TEXT("Error: Conversion of string failed for %s line %d.\n"), source, line);
         }
      else
         {
         // We explicitly write it out as a Unicode file.
         ::fputws(bufw, fpw);
         }
      }
   ::_fcloseall();

   return 0;
}
