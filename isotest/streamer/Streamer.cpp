#include "stdafx.h"
#include <windows.h>

// windows.h includes WINGDI.h which
// defines GetObject as GetObjectA, breaking
// System::Resources::ResourceManager::GetObject.
// So, we undef here.
#undef GetObject

#include "Streamer.h"

#undef MessageBox

using namespace System::Windows::Forms;
using namespace Streams;

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    System::Threading::Thread::CurrentThread->ApartmentState = System::Threading::ApartmentState::STA;

    try
    {
        Application::Run(new Form1());
    }
    catch (Exception *e)
    {
        MessageBox::Show(e->StackTrace,e->Message);
    }

    return 0;
}
