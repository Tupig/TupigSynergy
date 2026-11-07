/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Synergy Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsScreen.h"

#include "App.h"
#include "ClientApp.h"
#include "Clipboard.h"
#include "KeyMap.h"
#include "MSWindowsClipboard.h"
#include "MSWindowsDesks.h"
#include "MSWindowsDropTarget.h"
#include "MSWindowsEventQueueBuffer.h"
#include "MSWindowsKeyState.h"
#include "MSWindowsScreenSaver.h"
#include "ScreenException.h"
#include "Win32DropData.h"
#include "arch/Arch.h"
#include "arch/win32/ArchMiscWindows.h"
#include "arch/win32/XArchWindows.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/TMethodJob.h"
#include "client/Client.h"
#include "common/Constants.h"
#include "common/Settings.h"

#include <Shlobj.h>
#include <algorithm>
#include <comutil.h>
#include <string.h>

// suppress warning about GetVersionEx, which is used indirectly in this
// compilation unit.
#pragma warning(disable : 4996)

// X button stuff
#if !defined(WM_XBUTTONDOWN)
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_XBUTTONDBLCLK 0x020D
#define WM_NCXBUTTONDOWN 0x00AB
#define WM_NCXBUTTONUP 0x00AC
#define WM_NCXBUTTONDBLCLK 0x00AD
#define MOUSEEVENTF_XDOWN 0x0080
#define MOUSEEVENTF_XUP 0x0100
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#endif
#if !defined(VK_XBUTTON1)
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#endif

// WM_POWERBROADCAST stuff
#if !defined(PBT_APMRESUMEAUTOMATIC)
#define PBT_APMRESUMEAUTOMATIC 0x0012
#endif

//
// MSWindowsScreen
//

const std::vector<std::string> &MSWindowsScreen::draggingPaths() const
{
  static const std::vector<std::string> kEmpty;
  if (m_dropTarget == nullptr) {
    return kEmpty;
  }
  return m_dropTarget->draggingPaths();
}

std::vector<std::string> MSWindowsScreen::takeDraggingPaths()
{
  if (m_dropTarget == nullptr) {
    return {};
  }

  // When the user is holding the left button (typical Explorer drag), offer the
  // drop window under the cursor so DragEnter can fire before leave() warps away.
  if (m_buttons[kButtonLeft]) {
    offerDropWindowAtCursor();
  }

  std::vector<std::string> paths = m_dropTarget->draggingPaths();
  m_dropTarget->clearDraggingPaths();
  return paths;
}

bool MSWindowsScreen::startDraggingFiles(const std::vector<std::string> &paths)
{
  return synergy::win32::startDraggingFiles(paths);
}

void MSWindowsScreen::offerDropWindowAtCursor()
{
  if (m_dropWindow == nullptr || m_dropTarget == nullptr) {
    return;
  }

  POINT pt{};
  if (!::GetCursorPos(&pt)) {
    return;
  }

  const int half = m_dropWindowSize / 2;
  const int x = (pt.x - half) < 0 ? 0 : (pt.x - half);
  const int y = (pt.y - half) < 0 ? 0 : (pt.y - half);

  ::SetWindowPos(m_dropWindow, HWND_TOPMOST, x, y, m_dropWindowSize, m_dropWindowSize, SWP_SHOWWINDOW | SWP_NOACTIVATE);

  // Let OLE deliver DragEnter while the cursor is over our window. No Escape or
  // fake mouse-up: those were the unreliable bits upstream removed.
  const DWORD deadline = ::GetTickCount() + 200;
  while (::GetTickCount() < deadline) {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
    if (!m_dropTarget->draggingPaths().empty()) {
      break;
    }
    ::Sleep(10);
  }

  ::ShowWindow(m_dropWindow, SW_HIDE);
}

HCURSOR
MSWindowsScreen::createBlankCursor() const
{
  // create a transparent cursor
  int cw = GetSystemMetrics(SM_CXCURSOR);
  int ch = GetSystemMetrics(SM_CYCURSOR);

  uint8_t *cursorAND = new uint8_t[ch * ((cw + 31) >> 2)];
  uint8_t *cursorXOR = new uint8_t[ch * ((cw + 31) >> 2)];
  memset(cursorAND, 0xff, ch * ((cw + 31) >> 2));
  memset(cursorXOR, 0x00, ch * ((cw + 31) >> 2));
  HCURSOR c = CreateCursor(s_windowInstance, 0, 0, cw, ch, cursorAND, cursorXOR);
  delete[] cursorXOR;
  delete[] cursorAND;
  return c;
}

void MSWindowsScreen::destroyCursor(HCURSOR cursor) const
{
  if (cursor != nullptr) {
    DestroyCursor(cursor);
  }
}

ATOM MSWindowsScreen::createWindowClass() const
{
  WNDCLASSEX classInfo;
  classInfo.cbSize = sizeof(classInfo);
  classInfo.style = CS_DBLCLKS | CS_NOCLOSE;
  classInfo.lpfnWndProc = &MSWindowsScreen::wndProc;
  classInfo.cbClsExtra = 0;
  classInfo.cbWndExtra = 0;
  classInfo.hInstance = s_windowInstance;
  classInfo.hIcon = nullptr;
  classInfo.hCursor = nullptr;
  classInfo.hbrBackground = nullptr;
  classInfo.lpszMenuName = nullptr;
  classInfo.lpszClassName = LPCWSTR(kAppName);
  classInfo.hIconSm = nullptr;
  return RegisterClassEx(&classInfo);
}

void MSWindowsScreen::destroyClass(ATOM windowClass) const
{
  if (windowClass != 0) {
    UnregisterClass(MAKEINTATOM(windowClass), s_windowInstance);
  }
}

HWND MSWindowsScreen::createWindow(ATOM windowClass, const wchar_t *name) const
{
  HWND window = CreateWindowEx(
      WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, MAKEINTATOM(windowClass), name, WS_POPUP, 0, 0, 1, 1,
      nullptr, nullptr, s_windowInstance, nullptr
  );
  if (window == nullptr) {
    LOG_ERR("failed to create window: %d", GetLastError());
    throw ScreenOpenFailureException();
  }
  return window;
}

HWND MSWindowsScreen::createDropWindow(ATOM windowClass) const
{
  HWND window = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_ACCEPTFILES | WS_EX_TOOLWINDOW, MAKEINTATOM(windowClass), L"DropWindow",
      WS_POPUP, 0, 0, m_dropWindowSize, m_dropWindowSize, nullptr, nullptr, s_windowInstance, nullptr
  );
  if (window == nullptr) {
    LOG_ERR("failed to create drop window: %d", GetLastError());
    throw ScreenOpenFailureException();
  }
  return window;
}

void MSWindowsScreen::destroyWindow(HWND hwnd) const
{
  if (hwnd != nullptr) {
    DestroyWindow(hwnd);
  }
}

void MSWindowsScreen::updateScreenShape()
{
  // get shape and center
  m_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  m_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  m_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  m_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  m_xCenter = GetSystemMetrics(SM_CXSCREEN) >> 1;
  m_yCenter = GetSystemMetrics(SM_CYSCREEN) >> 1;

  // check for multiple monitors
  m_multimon = (m_w != GetSystemMetrics(SM_CXSCREEN) || m_h != GetSystemMetrics(SM_CYSCREEN));

  // tell the desks
  m_desks->setShape(m_x, m_y, m_w, m_h, m_xCenter, m_yCenter, m_multimon);
}
