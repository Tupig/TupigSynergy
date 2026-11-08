/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2022, 2025 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2003 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MSWindowsKeyState.h"

#include "MSWindowsDesks.h"
#include "MSWindowsHandle.h"
#include "arch/win32/XArchWindows.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "common/Constants.h"

// extended mouse buttons
#if !defined(VK_XBUTTON1)
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#endif

//
// MSWindowsKeyState
//

// map virtual keys to Synergy key enumeration
const KeyID MSWindowsKeyState::s_virtualKey[] = {
    /* 0x000 */ {kKeyNone},             // reserved
    /* 0x001 */ {kKeyNone},             // VK_LBUTTON
    /* 0x002 */ {kKeyNone},             // VK_RBUTTON
    /* 0x003 */ {kKeyNone},             // VK_CANCEL
    /* 0x004 */ {kKeyNone},             // VK_MBUTTON
    /* 0x005 */ {kKeyNone},             // VK_XBUTTON1
    /* 0x006 */ {kKeyNone},             // VK_XBUTTON2
    /* 0x007 */ {kKeyNone},             // undefined
    /* 0x008 */ {kKeyBackSpace},        // VK_BACK
    /* 0x009 */ {kKeyTab},              // VK_TAB
    /* 0x00a */ {kKeyNone},             // undefined
    /* 0x00b */ {kKeyNone},             // undefined
    /* 0x00c */ {kKeyClear},            // VK_CLEAR
    /* 0x00d */ {kKeyReturn},           // VK_RETURN
    /* 0x00e */ {kKeyNone},             // undefined
    /* 0x00f */ {kKeyNone},             // undefined
    /* 0x010 */ {kKeyShift_L},          // VK_SHIFT
    /* 0x011 */ {kKeyControl_L},        // VK_CONTROL
    /* 0x012 */ {kKeyAlt_L},            // VK_MENU
    /* 0x013 */ {kKeyPause},            // VK_PAUSE
    /* 0x014 */ {kKeyCapsLock},         // VK_CAPITAL
    /* 0x015 */ {kKeyKana},             // VK_HANGUL, VK_KANA
    /* 0x016 */ {kKeyNone},             // undefined
    /* 0x017 */ {kKeyNone},             // VK_JUNJA
    /* 0x018 */ {kKeyNone},             // VK_FINAL
    /* 0x019 */ {kKeyKanzi},            // VK_HANJA, VK_KANJI
    /* 0x01a */ {kKeyNone},             // undefined
    /* 0x01b */ {kKeyEscape},           // VK_ESCAPE
    /* 0x01c */ {kKeyHenkan},           // VK_CONVERT
    /* 0x01d */ {kKeyNone},             // VK_NONCONVERT
    /* 0x01e */ {kKeyNone},             // VK_ACCEPT
    /* 0x01f */ {kKeyNone},             // VK_MODECHANGE
    /* 0x020 */ {kKeyNone},             // VK_SPACE
    /* 0x021 */ {kKeyKP_PageUp},        // VK_PRIOR
    /* 0x022 */ {kKeyKP_PageDown},      // VK_NEXT
    /* 0x023 */ {kKeyKP_End},           // VK_END
    /* 0x024 */ {kKeyKP_Home},          // VK_HOME
    /* 0x025 */ {kKeyKP_Left},          // VK_LEFT
    /* 0x026 */ {kKeyKP_Up},            // VK_UP
    /* 0x027 */ {kKeyKP_Right},         // VK_RIGHT
    /* 0x028 */ {kKeyKP_Down},          // VK_DOWN
    /* 0x029 */ {kKeySelect},           // VK_SELECT
    /* 0x02a */ {kKeyNone},             // VK_PRINT
    /* 0x02b */ {kKeyExecute},          // VK_EXECUTE
    /* 0x02c */ {kKeyPrint},            // VK_SNAPSHOT
    /* 0x02d */ {kKeyKP_Insert},        // VK_INSERT
    /* 0x02e */ {kKeyKP_Delete},        // VK_DELETE
    /* 0x02f */ {kKeyHelp},             // VK_HELP
    /* 0x030 */ {kKeyNone},             // VK_0
    /* 0x031 */ {kKeyNone},             // VK_1
    /* 0x032 */ {kKeyNone},             // VK_2
    /* 0x033 */ {kKeyNone},             // VK_3
    /* 0x034 */ {kKeyNone},             // VK_4
    /* 0x035 */ {kKeyNone},             // VK_5
    /* 0x036 */ {kKeyNone},             // VK_6
    /* 0x037 */ {kKeyNone},             // VK_7
    /* 0x038 */ {kKeyNone},             // VK_8
    /* 0x039 */ {kKeyNone},             // VK_9
    /* 0x03a */ {kKeyNone},             // undefined
    /* 0x03b */ {kKeyNone},             // undefined
    /* 0x03c */ {kKeyNone},             // undefined
    /* 0x03d */ {kKeyNone},             // undefined
    /* 0x03e */ {kKeyNone},             // undefined
    /* 0x03f */ {kKeyNone},             // undefined
    /* 0x040 */ {kKeyNone},             // undefined
    /* 0x041 */ {kKeyNone},             // VK_A
    /* 0x042 */ {kKeyNone},             // VK_B
    /* 0x043 */ {kKeyNone},             // VK_C
    /* 0x044 */ {kKeyNone},             // VK_D
    /* 0x045 */ {kKeyNone},             // VK_E
    /* 0x046 */ {kKeyNone},             // VK_F
    /* 0x047 */ {kKeyNone},             // VK_G
    /* 0x048 */ {kKeyNone},             // VK_H
    /* 0x049 */ {kKeyNone},             // VK_I
    /* 0x04a */ {kKeyNone},             // VK_J
    /* 0x04b */ {kKeyNone},             // VK_K
    /* 0x04c */ {kKeyNone},             // VK_L
    /* 0x04d */ {kKeyNone},             // VK_M
    /* 0x04e */ {kKeyNone},             // VK_N
    /* 0x04f */ {kKeyNone},             // VK_O
    /* 0x050 */ {kKeyNone},             // VK_P
    /* 0x051 */ {kKeyNone},             // VK_Q
    /* 0x052 */ {kKeyNone},             // VK_R
    /* 0x053 */ {kKeyNone},             // VK_S
    /* 0x054 */ {kKeyNone},             // VK_T
    /* 0x055 */ {kKeyNone},             // VK_U
    /* 0x056 */ {kKeyNone},             // VK_V
    /* 0x057 */ {kKeyNone},             // VK_W
    /* 0x058 */ {kKeyNone},             // VK_X
    /* 0x059 */ {kKeyNone},             // VK_Y
    /* 0x05a */ {kKeyNone},             // VK_Z
    /* 0x05b */ {kKeySuper_L},          // VK_LWIN
    /* 0x05c */ {kKeySuper_R},          // VK_RWIN
    /* 0x05d */ {kKeyMenu},             // VK_APPS
    /* 0x05e */ {kKeyNone},             // undefined
    /* 0x05f */ {kKeySleep},            // VK_SLEEP
    /* 0x060 */ {kKeyKP_0},             // VK_NUMPAD0
    /* 0x061 */ {kKeyKP_1},             // VK_NUMPAD1
    /* 0x062 */ {kKeyKP_2},             // VK_NUMPAD2
    /* 0x063 */ {kKeyKP_3},             // VK_NUMPAD3
    /* 0x064 */ {kKeyKP_4},             // VK_NUMPAD4
    /* 0x065 */ {kKeyKP_5},             // VK_NUMPAD5
    /* 0x066 */ {kKeyKP_6},             // VK_NUMPAD6
    /* 0x067 */ {kKeyKP_7},             // VK_NUMPAD7
    /* 0x068 */ {kKeyKP_8},             // VK_NUMPAD8
    /* 0x069 */ {kKeyKP_9},             // VK_NUMPAD9
    /* 0x06a */ {kKeyKP_Multiply},      // VK_MULTIPLY
    /* 0x06b */ {kKeyKP_Add},           // VK_ADD
    /* 0x06c */ {kKeyKP_Separator},     // VK_SEPARATOR
    /* 0x06d */ {kKeyKP_Subtract},      // VK_SUBTRACT
    /* 0x06e */ {kKeyKP_Decimal},       // VK_DECIMAL
    /* 0x06f */ {kKeyNone},             // VK_DIVIDE
    /* 0x070 */ {kKeyF1},               // VK_F1
    /* 0x071 */ {kKeyF2},               // VK_F2
    /* 0x072 */ {kKeyF3},               // VK_F3
    /* 0x073 */ {kKeyF4},               // VK_F4
    /* 0x074 */ {kKeyF5},               // VK_F5
    /* 0x075 */ {kKeyF6},               // VK_F6
    /* 0x076 */ {kKeyF7},               // VK_F7
    /* 0x077 */ {kKeyF8},               // VK_F8
    /* 0x078 */ {kKeyF9},               // VK_F9
    /* 0x079 */ {kKeyF10},              // VK_F10
    /* 0x07a */ {kKeyF11},              // VK_F11
    /* 0x07b */ {kKeyF12},              // VK_F12
    /* 0x07c */ {kKeyF13},              // VK_F13
    /* 0x07d */ {kKeyF14},              // VK_F14
    /* 0x07e */ {kKeyF15},              // VK_F15
    /* 0x07f */ {kKeyF16},              // VK_F16
    /* 0x080 */ {kKeyF17},              // VK_F17
    /* 0x081 */ {kKeyF18},              // VK_F18
    /* 0x082 */ {kKeyF19},              // VK_F19
    /* 0x083 */ {kKeyF20},              // VK_F20
    /* 0x084 */ {kKeyF21},              // VK_F21
    /* 0x085 */ {kKeyF22},              // VK_F22
    /* 0x086 */ {kKeyF23},              // VK_F23
    /* 0x087 */ {kKeyF24},              // VK_F24
    /* 0x088 */ {kKeyNone},             // unassigned
    /* 0x089 */ {kKeyNone},             // unassigned
    /* 0x08a */ {kKeyNone},             // unassigned
    /* 0x08b */ {kKeyNone},             // unassigned
    /* 0x08c */ {kKeyNone},             // unassigned
    /* 0x08d */ {kKeyNone},             // unassigned
    /* 0x08e */ {kKeyNone},             // unassigned
    /* 0x08f */ {kKeyNone},             // unassigned
    /* 0x090 */ {kKeyNumLock},          // VK_NUMLOCK
    /* 0x091 */ {kKeyScrollLock},       // VK_SCROLL
    /* 0x092 */ {kKeyNone},             // unassigned
    /* 0x093 */ {kKeyNone},             // unassigned
    /* 0x094 */ {kKeyNone},             // unassigned
    /* 0x095 */ {kKeyNone},             // unassigned
    /* 0x096 */ {kKeyNone},             // unassigned
    /* 0x097 */ {kKeyNone},             // unassigned
    /* 0x098 */ {kKeyNone},             // unassigned
    /* 0x099 */ {kKeyNone},             // unassigned
    /* 0x09a */ {kKeyNone},             // unassigned
    /* 0x09b */ {kKeyNone},             // unassigned
    /* 0x09c */ {kKeyNone},             // unassigned
    /* 0x09d */ {kKeyNone},             // unassigned
    /* 0x09e */ {kKeyNone},             // unassigned
    /* 0x09f */ {kKeyNone},             // unassigned
    /* 0x0a0 */ {kKeyShift_L},          // VK_LSHIFT
    /* 0x0a1 */ {kKeyShift_R},          // VK_RSHIFT
    /* 0x0a2 */ {kKeyControl_L},        // VK_LCONTROL
    /* 0x0a3 */ {kKeyControl_R},        // VK_RCONTROL
    /* 0x0a4 */ {kKeyAlt_L},            // VK_LMENU
    /* 0x0a5 */ {kKeyAltGr},            // VK_RMENU
    /* 0x0a6 */ {kKeyNone},             // VK_BROWSER_BACK
    /* 0x0a7 */ {kKeyNone},             // VK_BROWSER_FORWARD
    /* 0x0a8 */ {kKeyNone},             // VK_BROWSER_REFRESH
    /* 0x0a9 */ {kKeyNone},             // VK_BROWSER_STOP
    /* 0x0aa */ {kKeyNone},             // VK_BROWSER_SEARCH
    /* 0x0ab */ {kKeyNone},             // VK_BROWSER_FAVORITES
    /* 0x0ac */ {kKeyNone},             // VK_BROWSER_HOME
    /* 0x0ad */ {kKeyNone},             // VK_VOLUME_MUTE
    /* 0x0ae */ {kKeyNone},             // VK_VOLUME_DOWN
    /* 0x0af */ {kKeyNone},             // VK_VOLUME_UP
    /* 0x0b0 */ {kKeyNone},             // VK_MEDIA_NEXT_TRACK
    /* 0x0b1 */ {kKeyNone},             // VK_MEDIA_PREV_TRACK
    /* 0x0b2 */ {kKeyNone},             // VK_MEDIA_STOP
    /* 0x0b3 */ {kKeyNone},             // VK_MEDIA_PLAY_PAUSE
    /* 0x0b4 */ {kKeyNone},             // VK_LAUNCH_MAIL
    /* 0x0b5 */ {kKeyNone},             // VK_LAUNCH_MEDIA_SELECT
    /* 0x0b6 */ {kKeyNone},             // VK_LAUNCH_APP1
    /* 0x0b7 */ {kKeyNone},             // VK_LAUNCH_APP2
    /* 0x0b8 */ {kKeyNone},             // unassigned
    /* 0x0b9 */ {kKeyNone},             // unassigned
    /* 0x0ba */ {kKeyNone},             // OEM specific
    /* 0x0bb */ {kKeyNone},             // OEM specific
    /* 0x0bc */ {kKeyNone},             // OEM specific
    /* 0x0bd */ {kKeyNone},             // OEM specific
    /* 0x0be */ {kKeyNone},             // OEM specific
    /* 0x0bf */ {kKeyNone},             // OEM specific
    /* 0x0c0 */ {kKeyNone},             // OEM specific
    /* 0x0c1 */ {kKeyNone},             // unassigned
    /* 0x0c2 */ {kKeyNone},             // unassigned
    /* 0x0c3 */ {kKeyNone},             // unassigned
    /* 0x0c4 */ {kKeyNone},             // unassigned
    /* 0x0c5 */ {kKeyNone},             // unassigned
    /* 0x0c6 */ {kKeyNone},             // unassigned
    /* 0x0c7 */ {kKeyNone},             // unassigned
    /* 0x0c8 */ {kKeyNone},             // unassigned
    /* 0x0c9 */ {kKeyNone},             // unassigned
    /* 0x0ca */ {kKeyNone},             // unassigned
    /* 0x0cb */ {kKeyNone},             // unassigned
    /* 0x0cc */ {kKeyNone},             // unassigned
    /* 0x0cd */ {kKeyNone},             // unassigned
    /* 0x0ce */ {kKeyNone},             // unassigned
    /* 0x0cf */ {kKeyNone},             // unassigned
    /* 0x0d0 */ {kKeyNone},             // unassigned
    /* 0x0d1 */ {kKeyNone},             // unassigned
    /* 0x0d2 */ {kKeyNone},             // unassigned
    /* 0x0d3 */ {kKeyNone},             // unassigned
    /* 0x0d4 */ {kKeyNone},             // unassigned
    /* 0x0d5 */ {kKeyNone},             // unassigned
    /* 0x0d6 */ {kKeyNone},             // unassigned
    /* 0x0d7 */ {kKeyNone},             // unassigned
    /* 0x0d8 */ {kKeyNone},             // unassigned
    /* 0x0d9 */ {kKeyNone},             // unassigned
    /* 0x0da */ {kKeyNone},             // unassigned
    /* 0x0db */ {kKeyNone},             // OEM specific
    /* 0x0dc */ {kKeyNone},             // OEM specific
    /* 0x0dd */ {kKeyNone},             // OEM specific
    /* 0x0de */ {kKeyNone},             // OEM specific
    /* 0x0df */ {kKeyNone},             // OEM specific
    /* 0x0e0 */ {kKeyNone},             // OEM specific
    /* 0x0e1 */ {kKeyNone},             // OEM specific
    /* 0x0e2 */ {kKeyNone},             // OEM specific
    /* 0x0e3 */ {kKeyNone},             // OEM specific
    /* 0x0e4 */ {kKeyNone},             // OEM specific
    /* 0x0e5 */ {kKeyNone},             // unassigned
    /* 0x0e6 */ {kKeyNone},             // OEM specific
    /* 0x0e7 */ {kKeyNone},             // unassigned
    /* 0x0e8 */ {kKeyNone},             // unassigned
    /* 0x0e9 */ {kKeyNone},             // OEM specific
    /* 0x0ea */ {kKeyNone},             // OEM specific
    /* 0x0eb */ {kKeyNone},             // OEM specific
    /* 0x0ec */ {kKeyNone},             // OEM specific
    /* 0x0ed */ {kKeyNone},             // OEM specific
    /* 0x0ee */ {kKeyNone},             // OEM specific
    /* 0x0ef */ {kKeyNone},             // OEM specific
    /* 0x0f0 */ {kKeyNone},             // OEM specific
    /* 0x0f1 */ {kKeyNone},             // OEM specific
    /* 0x0f2 */ {kKeyHiraganaKatakana}, // VK_OEM_COPY
    /* 0x0f3 */ {kKeyZenkaku},          // VK_OEM_AUTO
    /* 0x0f4 */ {kKeyZenkaku},          // VK_OEM_ENLW
    /* 0x0f5 */ {kKeyNone},             // OEM specific
    /* 0x0f6 */ {kKeyNone},             // VK_ATTN
    /* 0x0f7 */ {kKeyNone},             // VK_CRSEL
    /* 0x0f8 */ {kKeyNone},             // VK_EXSEL
    /* 0x0f9 */ {kKeyNone},             // VK_EREOF
    /* 0x0fa */ {kKeyNone},             // VK_PLAY
    /* 0x0fb */ {kKeyNone},             // VK_ZOOM
    /* 0x0fc */ {kKeyNone},             // reserved
    /* 0x0fd */ {kKeyNone},             // VK_PA1
    /* 0x0fe */ {kKeyNone},             // VK_OEM_CLEAR
    /* 0x0ff */ {kKeyNone},             // reserved

    /* 0x100 */ {kKeyNone},         // reserved
    /* 0x101 */ {kKeyNone},         // VK_LBUTTON
    /* 0x102 */ {kKeyNone},         // VK_RBUTTON
    /* 0x103 */ {kKeyBreak},        // VK_CANCEL
    /* 0x104 */ {kKeyNone},         // VK_MBUTTON
    /* 0x105 */ {kKeyNone},         // VK_XBUTTON1
    /* 0x106 */ {kKeyNone},         // VK_XBUTTON2
    /* 0x107 */ {kKeyNone},         // undefined
    /* 0x108 */ {kKeyNone},         // VK_BACK
    /* 0x109 */ {kKeyNone},         // VK_TAB
    /* 0x10a */ {kKeyNone},         // undefined
    /* 0x10b */ {kKeyNone},         // undefined
    /* 0x10c */ {kKeyClear},        // VK_CLEAR
    /* 0x10d */ {kKeyKP_Enter},     // VK_RETURN
    /* 0x10e */ {kKeyNone},         // undefined
    /* 0x10f */ {kKeyNone},         // undefined
    /* 0x110 */ {kKeyShift_R},      // VK_SHIFT
    /* 0x111 */ {kKeyControl_R},    // VK_CONTROL
    /* 0x112 */ {kKeyAlt_R},        // VK_MENU
    /* 0x113 */ {kKeyNone},         // VK_PAUSE
    /* 0x114 */ {kKeyNone},         // VK_CAPITAL
    /* 0x115 */ {kKeyHangul},       // VK_HANGUL
    /* 0x116 */ {kKeyNone},         // undefined
    /* 0x117 */ {kKeyNone},         // VK_JUNJA
    /* 0x118 */ {kKeyNone},         // VK_FINAL
    /* 0x119 */ {kKeyHanja},        // VK_HANJA
    /* 0x11a */ {kKeyNone},         // undefined
    /* 0x11b */ {kKeyNone},         // VK_ESCAPE
    /* 0x11c */ {kKeyNone},         // VK_CONVERT
    /* 0x11d */ {kKeyNone},         // VK_NONCONVERT
    /* 0x11e */ {kKeyNone},         // VK_ACCEPT
    /* 0x11f */ {kKeyNone},         // VK_MODECHANGE
    /* 0x120 */ {kKeyNone},         // VK_SPACE
    /* 0x121 */ {kKeyPageUp},       // VK_PRIOR
    /* 0x122 */ {kKeyPageDown},     // VK_NEXT
    /* 0x123 */ {kKeyEnd},          // VK_END
    /* 0x124 */ {kKeyHome},         // VK_HOME
    /* 0x125 */ {kKeyLeft},         // VK_LEFT
    /* 0x126 */ {kKeyUp},           // VK_UP
    /* 0x127 */ {kKeyRight},        // VK_RIGHT
    /* 0x128 */ {kKeyDown},         // VK_DOWN
    /* 0x129 */ {kKeySelect},       // VK_SELECT
    /* 0x12a */ {kKeyNone},         // VK_PRINT
    /* 0x12b */ {kKeyExecute},      // VK_EXECUTE
    /* 0x12c */ {kKeyPrint},        // VK_SNAPSHOT
    /* 0x12d */ {kKeyInsert},       // VK_INSERT
    /* 0x12e */ {kKeyDelete},       // VK_DELETE
    /* 0x12f */ {kKeyHelp},         // VK_HELP
    /* 0x130 */ {kKeyNone},         // VK_0
    /* 0x131 */ {kKeyNone},         // VK_1
    /* 0x132 */ {kKeyNone},         // VK_2
    /* 0x133 */ {kKeyNone},         // VK_3
    /* 0x134 */ {kKeyNone},         // VK_4
    /* 0x135 */ {kKeyNone},         // VK_5
    /* 0x136 */ {kKeyNone},         // VK_6
    /* 0x137 */ {kKeyNone},         // VK_7
    /* 0x138 */ {kKeyNone},         // VK_8
    /* 0x139 */ {kKeyNone},         // VK_9
    /* 0x13a */ {kKeyNone},         // undefined
    /* 0x13b */ {kKeyNone},         // undefined
    /* 0x13c */ {kKeyNone},         // undefined
    /* 0x13d */ {kKeyNone},         // undefined
    /* 0x13e */ {kKeyNone},         // undefined
    /* 0x13f */ {kKeyNone},         // undefined
    /* 0x140 */ {kKeyNone},         // undefined
    /* 0x141 */ {kKeyNone},         // VK_A
    /* 0x142 */ {kKeyNone},         // VK_B
    /* 0x143 */ {kKeyNone},         // VK_C
    /* 0x144 */ {kKeyNone},         // VK_D
    /* 0x145 */ {kKeyNone},         // VK_E
    /* 0x146 */ {kKeyNone},         // VK_F
    /* 0x147 */ {kKeyNone},         // VK_G
    /* 0x148 */ {kKeyNone},         // VK_H
    /* 0x149 */ {kKeyNone},         // VK_I
    /* 0x14a */ {kKeyNone},         // VK_J
    /* 0x14b */ {kKeyNone},         // VK_K
    /* 0x14c */ {kKeyNone},         // VK_L
    /* 0x14d */ {kKeyNone},         // VK_M
    /* 0x14e */ {kKeyNone},         // VK_N
    /* 0x14f */ {kKeyNone},         // VK_O
    /* 0x150 */ {kKeyNone},         // VK_P
    /* 0x151 */ {kKeyNone},         // VK_Q
    /* 0x152 */ {kKeyNone},         // VK_R
    /* 0x153 */ {kKeyNone},         // VK_S
    /* 0x154 */ {kKeyNone},         // VK_T
    /* 0x155 */ {kKeyNone},         // VK_U
    /* 0x156 */ {kKeyNone},         // VK_V
    /* 0x157 */ {kKeyNone},         // VK_W
    /* 0x158 */ {kKeyNone},         // VK_X
    /* 0x159 */ {kKeyNone},         // VK_Y
    /* 0x15a */ {kKeyNone},         // VK_Z
    /* 0x15b */ {kKeySuper_L},      // VK_LWIN
    /* 0x15c */ {kKeySuper_R},      // VK_RWIN
    /* 0x15d */ {kKeyMenu},         // VK_APPS
    /* 0x15e */ {kKeyNone},         // undefined
    /* 0x15f */ {kKeyNone},         // VK_SLEEP
    /* 0x160 */ {kKeyNone},         // VK_NUMPAD0
    /* 0x161 */ {kKeyNone},         // VK_NUMPAD1
    /* 0x162 */ {kKeyNone},         // VK_NUMPAD2
    /* 0x163 */ {kKeyNone},         // VK_NUMPAD3
    /* 0x164 */ {kKeyNone},         // VK_NUMPAD4
    /* 0x165 */ {kKeyNone},         // VK_NUMPAD5
    /* 0x166 */ {kKeyNone},         // VK_NUMPAD6
    /* 0x167 */ {kKeyNone},         // VK_NUMPAD7
    /* 0x168 */ {kKeyNone},         // VK_NUMPAD8
    /* 0x169 */ {kKeyNone},         // VK_NUMPAD9
    /* 0x16a */ {kKeyNone},         // VK_MULTIPLY
    /* 0x16b */ {kKeyNone},         // VK_ADD
    /* 0x16c */ {kKeyKP_Separator}, // VK_SEPARATOR
    /* 0x16d */ {kKeyNone},         // VK_SUBTRACT
    /* 0x16e */ {kKeyNone},         // VK_DECIMAL
    /* 0x16f */ {kKeyKP_Divide},    // VK_DIVIDE
    /* 0x170 */ {kKeyNone},         // VK_F1
    /* 0x171 */ {kKeyNone},         // VK_F2
    /* 0x172 */ {kKeyNone},         // VK_F3
    /* 0x173 */ {kKeyNone},         // VK_F4
    /* 0x174 */ {kKeyNone},         // VK_F5
    /* 0x175 */ {kKeyNone},         // VK_F6
    /* 0x176 */ {kKeyNone},         // VK_F7
    /* 0x177 */ {kKeyNone},         // VK_F8
    /* 0x178 */ {kKeyNone},         // VK_F9
    /* 0x179 */ {kKeyNone},         // VK_F10
    /* 0x17a */ {kKeyNone},         // VK_F11
    /* 0x17b */ {kKeyNone},         // VK_F12
    /* 0x17c */ {kKeyF13},          // VK_F13
    /* 0x17d */ {kKeyF14},          // VK_F14
    /* 0x17e */ {kKeyF15},          // VK_F15
    /* 0x17f */ {kKeyF16},          // VK_F16
    /* 0x180 */ {kKeyF17},          // VK_F17
    /* 0x181 */ {kKeyF18},          // VK_F18
    /* 0x182 */ {kKeyF19},          // VK_F19
    /* 0x183 */ {kKeyF20},          // VK_F20
    /* 0x184 */ {kKeyF21},          // VK_F21
    /* 0x185 */ {kKeyF22},          // VK_F22
    /* 0x186 */ {kKeyF23},          // VK_F23
    /* 0x187 */ {kKeyF24},          // VK_F24
    /* 0x188 */ {kKeyNone},         // unassigned
    /* 0x189 */ {kKeyNone},         // unassigned
    /* 0x18a */ {kKeyNone},         // unassigned
    /* 0x18b */ {kKeyNone},         // unassigned
    /* 0x18c */ {kKeyNone},         // unassigned
    /* 0x18d */ {kKeyNone},         // unassigned
    /* 0x18e */ {kKeyNone},         // unassigned
    /* 0x18f */ {kKeyNone},         // unassigned
    /* 0x190 */ {kKeyNumLock},      // VK_NUMLOCK
    /* 0x191 */ {kKeyNone},         // VK_SCROLL
    /* 0x192 */ {kKeyNone},         // unassigned
    /* 0x193 */ {kKeyNone},         // unassigned
    /* 0x194 */ {kKeyNone},         // unassigned
    /* 0x195 */ {kKeyNone},         // unassigned
    /* 0x196 */ {kKeyNone},         // unassigned
    /* 0x197 */ {kKeyNone},         // unassigned
    /* 0x198 */ {kKeyNone},         // unassigned
    /* 0x199 */ {kKeyNone},         // unassigned
    /* 0x19a */ {kKeyNone},         // unassigned
    /* 0x19b */ {kKeyNone},         // unassigned
    /* 0x19c */ {kKeyNone},         // unassigned
    /* 0x19d */ {kKeyNone},         // unassigned
    /* 0x19e */ {kKeyNone},         // unassigned
    /* 0x19f */ {kKeyNone},         // unassigned
    /* 0x1a0 */ {kKeyShift_L},      // VK_LSHIFT
    /* 0x1a1 */ {kKeyShift_R},      // VK_RSHIFT
    /* 0x1a2 */ {kKeyControl_L},    // VK_LCONTROL
    /* 0x1a3 */ {kKeyControl_R},    // VK_RCONTROL
    /* 0x1a4 */ {kKeyAlt_L},        // VK_LMENU
    /* 0x1a5 */ {kKeyAltGr},        // VK_RMENU
    /* 0x1a6 */ {kKeyWWWBack},      // VK_BROWSER_BACK
    /* 0x1a7 */ {kKeyWWWForward},   // VK_BROWSER_FORWARD
    /* 0x1a8 */ {kKeyWWWRefresh},   // VK_BROWSER_REFRESH
    /* 0x1a9 */ {kKeyWWWStop},      // VK_BROWSER_STOP
    /* 0x1aa */ {kKeyWWWSearch},    // VK_BROWSER_SEARCH
    /* 0x1ab */ {kKeyWWWFavorites}, // VK_BROWSER_FAVORITES
    /* 0x1ac */ {kKeyWWWHome},      // VK_BROWSER_HOME
    /* 0x1ad */ {kKeyAudioMute},    // VK_VOLUME_MUTE
    /* 0x1ae */ {kKeyAudioDown},    // VK_VOLUME_DOWN
    /* 0x1af */ {kKeyAudioUp},      // VK_VOLUME_UP
    /* 0x1b0 */ {kKeyAudioNext},    // VK_MEDIA_NEXT_TRACK
    /* 0x1b1 */ {kKeyAudioPrev},    // VK_MEDIA_PREV_TRACK
    /* 0x1b2 */ {kKeyAudioStop},    // VK_MEDIA_STOP
    /* 0x1b3 */ {kKeyAudioPlay},    // VK_MEDIA_PLAY_PAUSE
    /* 0x1b4 */ {kKeyAppMail},      // VK_LAUNCH_MAIL
    /* 0x1b5 */ {kKeyAppMedia},     // VK_LAUNCH_MEDIA_SELECT
    /* 0x1b6 */ {kKeyAppUser1},     // VK_LAUNCH_APP1
    /* 0x1b7 */ {kKeyAppUser2},     // VK_LAUNCH_APP2
    /* 0x1b8 */ {kKeyNone},         // unassigned
    /* 0x1b9 */ {kKeyNone},         // unassigned
    /* 0x1ba */ {kKeyNone},         // OEM specific
    /* 0x1bb */ {kKeyNone},         // OEM specific
    /* 0x1bc */ {kKeyNone},         // OEM specific
    /* 0x1bd */ {kKeyNone},         // OEM specific
    /* 0x1be */ {kKeyNone},         // OEM specific
    /* 0x1bf */ {kKeyNone},         // OEM specific
    /* 0x1c0 */ {kKeyNone},         // OEM specific
    /* 0x1c1 */ {kKeyNone},         // unassigned
    /* 0x1c2 */ {kKeyNone},         // unassigned
    /* 0x1c3 */ {kKeyNone},         // unassigned
    /* 0x1c4 */ {kKeyNone},         // unassigned
    /* 0x1c5 */ {kKeyNone},         // unassigned
    /* 0x1c6 */ {kKeyNone},         // unassigned
    /* 0x1c7 */ {kKeyNone},         // unassigned
    /* 0x1c8 */ {kKeyNone},         // unassigned
    /* 0x1c9 */ {kKeyNone},         // unassigned
    /* 0x1ca */ {kKeyNone},         // unassigned
    /* 0x1cb */ {kKeyNone},         // unassigned
    /* 0x1cc */ {kKeyNone},         // unassigned
    /* 0x1cd */ {kKeyNone},         // unassigned
    /* 0x1ce */ {kKeyNone},         // unassigned
    /* 0x1cf */ {kKeyNone},         // unassigned
    /* 0x1d0 */ {kKeyNone},         // unassigned
    /* 0x1d1 */ {kKeyNone},         // unassigned
    /* 0x1d2 */ {kKeyNone},         // unassigned
    /* 0x1d3 */ {kKeyNone},         // unassigned
    /* 0x1d4 */ {kKeyNone},         // unassigned
    /* 0x1d5 */ {kKeyNone},         // unassigned
    /* 0x1d6 */ {kKeyNone},         // unassigned
    /* 0x1d7 */ {kKeyNone},         // unassigned
    /* 0x1d8 */ {kKeyNone},         // unassigned
    /* 0x1d9 */ {kKeyNone},         // unassigned
    /* 0x1da */ {kKeyNone},         // unassigned
    /* 0x1db */ {kKeyNone},         // OEM specific
    /* 0x1dc */ {kKeyNone},         // OEM specific
    /* 0x1dd */ {kKeyNone},         // OEM specific
    /* 0x1de */ {kKeyNone},         // OEM specific
    /* 0x1df */ {kKeyNone},         // OEM specific
    /* 0x1e0 */ {kKeyNone},         // OEM specific
    /* 0x1e1 */ {kKeyNone},         // OEM specific
    /* 0x1e2 */ {kKeyNone},         // OEM specific
    /* 0x1e3 */ {kKeyNone},         // OEM specific
    /* 0x1e4 */ {kKeyNone},         // OEM specific
    /* 0x1e5 */ {kKeyNone},         // unassigned
    /* 0x1e6 */ {kKeyNone},         // OEM specific
    /* 0x1e7 */ {kKeyNone},         // unassigned
    /* 0x1e8 */ {kKeyNone},         // unassigned
    /* 0x1e9 */ {kKeyNone},         // OEM specific
    /* 0x1ea */ {kKeyNone},         // OEM specific
    /* 0x1eb */ {kKeyNone},         // OEM specific
    /* 0x1ec */ {kKeyNone},         // OEM specific
    /* 0x1ed */ {kKeyNone},         // OEM specific
    /* 0x1ee */ {kKeyNone},         // OEM specific
    /* 0x1ef */ {kKeyNone},         // OEM specific
    /* 0x1f0 */ {kKeyNone},         // OEM specific
    /* 0x1f1 */ {kKeyNone},         // OEM specific
    /* 0x1f2 */ {kKeyNone},         // VK_OEM_COPY
    /* 0x1f3 */ {kKeyNone},         // VK_OEM_AUTO
    /* 0x1f4 */ {kKeyNone},         // VK_OEM_ENLW
    /* 0x1f5 */ {kKeyNone},         // OEM specific
    /* 0x1f6 */ {kKeyNone},         // VK_ATTN
    /* 0x1f7 */ {kKeyNone},         // VK_CRSEL
    /* 0x1f8 */ {kKeyNone},         // VK_EXSEL
    /* 0x1f9 */ {kKeyNone},         // VK_EREOF
    /* 0x1fa */ {kKeyNone},         // VK_PLAY
    /* 0x1fb */ {kKeyNone},         // VK_ZOOM
    /* 0x1fc */ {kKeyNone},         // reserved
    /* 0x1fd */ {kKeyNone},         // VK_PA1
    /* 0x1fe */ {kKeyNone},         // VK_OEM_CLEAR
    /* 0x1ff */ {kKeyNone}          // reserved
};
