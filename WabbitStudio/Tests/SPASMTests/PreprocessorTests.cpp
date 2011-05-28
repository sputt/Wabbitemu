#pragma unmanaged

#pragma comment(lib, "spasm.lib")
#include "..\..\Source\SPASM\pass_one.h"
#include "..\..\Source\SPASM\pass_two.h"
#include "..\..\Source\SPASM\parser.h"
#include "..\..\Source\SPASM\spasm.h"
#include "..\..\Source\SPASM\errors.h"

#include <stdio.h>
#include <direct.h>
//#include <string>

#pragma managed

using namespace System;
using namespace System::Text;
using namespace System::Collections::Generic;
using namespace	Microsoft::VisualStudio::TestTools::UnitTesting;



using namespace std;

namespace SPASMTestsVS2008
{
	[TestClass]
	public ref class PreprocessorTests
	{
	public: 
		[TestInitialize]
		void Init()
		{
			OutputDebugString(TEXT("Start\n"));
			output_contents = (unsigned char *) malloc(output_buf_size);
			init_storage();
			curr_input_file = "..\\..\\..\\..\\..\\Tests\\SPASMTests\\PreprocessorTests.z80";
			output_filename = "output.bin";
			mode = MODE_LIST;

			char buffer[256];
			_getcwd(buffer, sizeof(buffer));

			int nResult = run_assembly();
			Assert::AreEqual((int) EXIT_NORMAL, nResult, "Could not assemble test file");

			ClearSPASMErrorSessions();
			mode = 0;
		}

		[TestCleanup]
		void Cleanup()
		{
			OutputDebugString(TEXT("End\n"));
			free_storage();
			free(output_contents);
		}

		void RunTest(const char *function_name)
		{
			char szFunctionName[256];
			strcpy_s(szFunctionName, strrchr(function_name, ':') + 1);

			char buffer[256];
			sprintf_s(buffer, " %s()", szFunctionName);
			
			TCHAR szFileName[256];
			sprintf_s(szFileName, "%s.txt", szFunctionName);
			add_define (strdup ("OUTPUT_FILE"), NULL)->contents = strdup (szFileName);

			ClearSPASMErrorSessions();
			int session = StartSPASMErrorSession();
			run_first_pass(buffer);
			EndSPASMErrorSession(session);

			String ^sfunction = gcnew String(szFileName);
			System::IO::StreamReader ^sr = gcnew System::IO::StreamReader(sfunction);
			System::String ^str = sr->ReadLine();

			Assert::AreEqual(gcnew String("PASS"), str);
		}

		[TestMethod]
		void Ifdef()
		{
			RunTest(__FUNCTION__);
		}

		[TestMethod]
		void Ifndef()
		{
			RunTest(__FUNCTION__);
		}

		[TestMethod]
		void Else()
		{
			RunTest(__FUNCTION__);
		}

		[TestMethod]
		void If()
		{
			RunTest(__FUNCTION__);
		}

		[TestMethod]
		void Elif()
		{
			RunTest(__FUNCTION__);
		}
	};
}