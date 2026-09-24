/*
 * Project Ambrose by Imjustchico
 * Unit test entry point that initializes GoogleTest and GoogleMock and runs every test, having first told the Microsoft debug runtime to report a failed assertion or a corrupted heap on standard error rather than in a window, because a window waits for somebody to click it and there is nobody there: a run that would have failed in a second otherwise hangs until it is killed, and the reason it failed is on a screen no log keeps.
 */

#include <gmock/gmock.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main(int argc, char** argv)
{
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    for (int const report : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
    {
        _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
    }
#endif
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
