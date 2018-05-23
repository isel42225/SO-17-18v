#include "stdafx.h"
#include "CppUnitTest.h"
#include "../Include/UThread.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UthreadsTest
{
	TEST_CLASS(UnitTest1)
	{
	private:
		//thread function for test RunningThreadIsAliveTest
		static VOID Func1(UT_ARGUMENT arg)
		{
			PBOOL isAlive = (PBOOL)arg;
			*isAlive = UtAlive(UtSelf());
		}

	public:

		TEST_METHOD(RunningThreadIsAliveTest)
		{
			UtInit();
			BOOL isAlive = FALSE;
			UtCreate(Func1, &isAlive);
			UtRun();
			Assert::IsTrue(isAlive == TRUE);
			UtEnd();

		}

	};
}