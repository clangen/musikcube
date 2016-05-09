//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Björn Olievier
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright 
//      notice, this list of conditions and the following disclaimer in the 
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may 
//      be used to endorse or promote products derived from this software 
//      without specific prior written permission. 
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE. 
//
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "Colors.h"
#include "LogWindow.h"
#include "CommandWindow.h"
#include "OutputWindow.h"
#include "TransportWindow.h"
#include "ResourcesWindow.h"
#include "IInput.h"

#include <boost/locale.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

#ifdef WIN32
#undef MOUSE_MOVED

int _main(int argc, _TCHAR* argv[]);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    _main(0, 0);
}

int _main(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    /* the following allows boost::filesystem to use utf8 on Windows */
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);
    
    musik::debug::init();

    ttytype[0] = 30; /* min height */
    ttytype[1] = 30; /* max height */
    ttytype[2] = 120; /* min width */
    ttytype[3] = 120; /* max width */

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    refresh();
    curs_set(0);

#ifdef __PDCURSES__
    PDC_set_title("musikbox ♫");
#endif

    {
        Transport tp;
        tp.SetVolume(0.01);

        Colors::Init();
        LogWindow logs;
        OutputWindow output;
        ResourcesWindow resources;
        CommandWindow command(tp, output);
        TransportWindow transport(tp);

        std::vector<BorderedWindow*> order;
        order.push_back(&command);
        order.push_back(&logs);
        order.push_back(&output);

        size_t index = 0;
        BorderedWindow *focused = order.at(index);
        focused->SetBorderColor(BOX_COLOR_RED_ON_BLACK);

        int ch;
        timeout(500);
        while (ch = getch()) {
            ScrollableWindow *scrollable = dynamic_cast<ScrollableWindow*>(focused);
            IInput *input = dynamic_cast<IInput*>(focused);

            if (ch == -1) { /* timeout */
                logs.Update();
                transport.Repaint();
                resources.Repaint();
            }
            else if (ch == 9) { /* tab */
                focused->SetBorderColor(BOX_COLOR_WHITE_ON_BLACK);

                index++;
                if (index >= order.size()) {
                    index = 0;
                }

                focused = order.at(index);
                focused->SetBorderColor(BOX_COLOR_RED_ON_BLACK);
            }
            else if (ch >= KEY_F(0) && ch <= KEY_F(12)) {
            }
            else if (ch == KEY_NPAGE) {
                if (scrollable) {
                    scrollable->PageDown();
                }
            }
            else if (ch == KEY_PPAGE) {
                if (scrollable) {
                    scrollable->PageUp();
                }
            }
            else if (ch == KEY_DOWN) {
                if (scrollable) {
                    scrollable->ScrollDown();
                }
            }
            else if (ch == KEY_UP) {
                if (scrollable) {
                    scrollable->ScrollUp();
                }
            }
            else if (input) {
                input->WriteChar(ch);
            }
        }
    }

    endwin();

    musik::debug::deinit();

    return 0;
}

