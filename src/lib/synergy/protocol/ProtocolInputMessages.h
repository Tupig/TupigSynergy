/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
/**
 * @defgroup protocol_data Data Messages
 * @brief Input events, clipboard data, and file transfer messages
 * @{
 */

/**
 * @defgroup protocol_keyboard Keyboard Messages
 * @brief Keyboard input event messages
 * @{
 */

/**
 * @brief Key press with language code (v1.8+)
 *
 * **Message Code**: `"DKDL"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKDL%2i%2i%2i%s"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier, often called a "keysym" on Linux/X11. This is platform-dependent
 * and corresponds to values like `XK_a` on X11/Linux, `'a'` on macOS, and `'A'` on Windows.
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 * - `$3`: KeyButton (2 bytes) - Physical key code, often called a "keycode" or "scancode". This is the raw,
 * platform-dependent scan code of the key pressed.
 * - `$4`: Language code (string) - Keyboard language identifier
 *
 * **Example**:
 *
 * 'a' key (KeyID 0x61), no modifiers, physical key (KeyButton 0x1E), English
 * ```
 * "DKDL\x00\x61\x00\x00\x00\x1E\x00\x00\x00\x02en"
 * ```
 *
 * Enhanced version of kMsgDKeyDown that includes language information
 * to help clients handle unknown language characters correctly.
 *
 * @see kMsgDKeyDown
 * @since Protocol version 1.8
 */
extern const char *const kMsgDKeyDownLang;

/**
 * @brief Key press event
 *
 * **Message Code**: `"DKDN"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKDN%2i%2i%2i"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier, often called a "keysym" on Linux/X11. This is platform-dependent
 * and corresponds to values like `XK_a` on X11/Linux, `'a'` on macOS, and `'A'` on Windows.
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 * - `$3`: KeyButton (2 bytes) - Physical key code, often called a "keycode" or "scancode". This is the raw,
 * platform-dependent scan code of the key pressed.
 *
 * **Example**:
 *
 * 'a' key (KeyID 0x61), no modifiers, physical key (KeyButton 0x1E)
 * ```
 * "DKDN\x00\x61\x00\x00\x00\x1E"
 * ```
 *
 * **Key Mapping Strategy**:
 * The KeyButton parameter is crucial for proper key release handling.
 * The secondary screen should:
 * 1. Map the KeyID to its local virtual key
 * 2. Remember the association between KeyButton and the local physical key
 * 3. Use KeyButton (not KeyID) to identify which key to release
 *
 * This is necessary because:
 * - Dead keys can change the KeyID between press and release
 * - Different keyboard layouts may produce different KeyIDs
 * - Modifier keys released before the main key can alter KeyID
 *
 * @see kMsgDKeyUp, kMsgDKeyDownLang
 * @since Protocol version 1.1
 */
extern const char *const kMsgDKeyDown;

/**
 * @brief Key press event (legacy v1.0)
 *
 * **Message Code**: `"DKDN"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKDN%2i%2i"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 *
 * Legacy version without KeyButton parameter. Used only when
 * communicating with protocol version 1.0 clients.
 *
 * @deprecated Use kMsgDKeyDown for protocol version 1.1+
 * @see kMsgDKeyDown
 * @since Protocol version 1.0
 */
extern const char *const kMsgDKeyDown1_0;

/**
 * @brief Key auto-repeat event
 *
 * **Message Code**: `"DKRP"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKRP%2i%2i%2i%2i%s"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier, often called a "keysym" on Linux/X11. This is platform-dependent
 * and corresponds to values like `XK_a` on X11/Linux, `'a'` on macOS, and `'A'` on Windows.
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 * - `$3`: Repeat count (2 bytes) - Number of repeats
 * - `$4`: KeyButton (2 bytes) - Physical key code, often called a "keycode" or "scancode". This is the raw,
 * platform-dependent scan code of the key pressed.
 * - `$5`: Language code (string) - Keyboard language identifier
 *
 * **Example**:
 *
 * 'a' key repeating 3 times, English layout
 * ```
 * "DKRP\x00\x61\x00\x00\x00\x03\x00\x1E\x00\x00\x00\x02en"
 * ```
 *
 * Sent when a key is held down and auto-repeating. The repeat count
 * indicates how many repeat events occurred since the last message.
 *
 * @see kMsgDKeyDown
 * @since Protocol version 1.1
 */
extern const char *const kMsgDKeyRepeat;

/**
 * @brief Key auto-repeat event (legacy v1.0)
 *
 * **Message Code**: `"DKRP"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKRP%2i%2i%2i"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 * - `$3`: Repeat count (2 bytes) - Number of repeats
 *
 * Legacy version without KeyButton and language parameters.
 *
 * @deprecated Use kMsgDKeyRepeat for protocol version 1.1+
 * @see kMsgDKeyRepeat
 * @since Protocol version 1.0
 */
extern const char *const kMsgDKeyRepeat1_0;

/**
 * @brief Key release event
 *
 * **Message Code**: `"DKUP"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKUP%2i%2i%2i"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier, often called a "keysym" on Linux/X11. This is platform-dependent
 * and corresponds to values like `XK_a` on X11/Linux, `'a'` on macOS, and `'A'` on Windows.
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 * - `$3`: KeyButton (2 bytes) - Physical key code, often called a "keycode" or "scancode". This is the raw,
 * platform-dependent scan code of the key pressed.
 *
 * **Example**:
 *
 * Release 'a' key, physical key 0x1E
 * ```
 * "DKUP\x00\x61\x00\x00\x00\x1E"
 * ```
 *
 * **Important**: The secondary screen should use KeyButton (not KeyID)
 * to determine which physical key to release. This ensures correct
 * behavior with dead keys and layout differences.
 *
 * @see kMsgDKeyDown
 * @since Protocol version 1.1
 */
extern const char *const kMsgDKeyUp;

/**
 * @brief Key release event (legacy v1.0)
 *
 * **Message Code**: `"DKUP"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DKUP%2i%2i"`
 * **Parameters**:
 * - `$1`: KeyID (2 bytes) - Virtual key identifier
 * - `$2`: KeyModifierMask (2 bytes) - Active modifier keys
 *
 * Legacy version without KeyButton parameter.
 *
 * @deprecated Use kMsgDKeyUp for protocol version 1.1+
 * @see kMsgDKeyUp
 * @since Protocol version 1.0
 */
extern const char *const kMsgDKeyUp1_0;

/** @} */ // end of protocol_keyboard group

/**
 * @defgroup protocol_mouse Mouse Messages
 * @brief Mouse input event messages
 * @{
 */

/**
 * @brief Mouse button press event
 *
 * **Message Code**: `"DMDN"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DMDN%1i"`
 * **Parameters**:
 * - `$1`: ButtonID (1 byte) - Mouse button identifier
 *
 * **Examples**:
 *
 * Left mouse button pressed
 * ```
 * "DMDN\x01"
 * ```
 *
 * Right mouse button pressed
 * ```
 * "DMDN\x02"
 * ```
 *
 * Middle mouse button pressed
 * ```
 * "DMDN\x03"
 * ```
 *
 * **Button IDs**:
 * - `1`: Left button
 * - `2`: Right button
 * - `3`: Middle button
 * - `4+`: Additional buttons (side buttons, etc.)
 *
 * @see kMsgDMouseUp
 * @since Protocol version 1.0
 */
extern const char *const kMsgDMouseDown;

/**
 * @brief Mouse button release event
 *
 * **Message Code**: `"DMUP"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DMUP%1i"`
 * **Parameters**:
 * - `$1`: ButtonID (1 byte) - Mouse button identifier
 *
 * **Example**:
 *
 * Left mouse button released
 * ```
 * "DMUP\x01"
 * ```
 *
 * Button IDs are the same as for kMsgDMouseDown.
 *
 * @see kMsgDMouseDown
 * @since Protocol version 1.0
 */
extern const char *const kMsgDMouseUp;

/**
 * @brief Absolute mouse movement
 *
 * **Message Code**: `"DMMV"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DMMV%2i%2i"`
 * **Parameters**:
 * - `$1`: X coordinate (2 bytes, signed) - Absolute screen position
 * - `$2`: Y coordinate (2 bytes, signed) - Absolute screen position
 *
 * **Example**:
 *
 * Move to (400, 300)
 * ```
 * "DMMV\x01\x90\x01\x2C"
 * ```
 *
 * Coordinates are absolute positions on the secondary screen.
 * The origin (0,0) is typically the top-left corner.
 *
 * @see kMsgDMouseRelMove
 * @since Protocol version 1.0
 */
extern const char *const kMsgDMouseMove;

/**
 * @brief Relative mouse movement
 *
 * **Message Code**: `"DMRM"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DMRM%2i%2i"`
 * **Parameters**:
 * - `$1`: X delta (2 bytes, signed) - Horizontal movement
 * - `$2`: Y delta (2 bytes, signed) - Vertical movement
 *
 * **Example**:
 *
 * Move right 10, up 10 pixels
 * ```
 * "DMRM\x00\x0A\xFF\xF6"
 * ```
 *
 * Relative movement is useful for:
 * - High-precision input devices
 * - Gaming applications
 * - When absolute positioning is not desired
 *
 * @see kMsgDMouseMove
 * @since Protocol version 1.2
 */
extern const char *const kMsgDMouseRelMove;

/**
 * @brief Mouse wheel scroll event
 *
 * **Message Code**: `"DMWM"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DMWM%2i%2i"`
 * **Parameters**:
 * - `$1`: X delta (2 bytes, signed) - Horizontal scroll
 * - `$2`: Y delta (2 bytes, signed) - Vertical scroll
 *
 * **Example**:
 *
 * Scroll up one tick (+120)
 * ```
 * "DMWM\x00\x00\x00\x78"
 * ```
 *
 * Scroll down one tick (-120)
 * ```
 * "DMWM\x00\x00\xFF\x88"
 * ```
 *
 * Scroll right one tick (+120)
 * ```
 * "DMWM\x00\x78\x00\x00"
 * ```
 *
 * **Scroll Values**:
 * - `+120`: One tick forward (away from user) or right
 * - `-120`: One tick backward (toward user) or left
 * - Values are typically multiples of 120
 *
 * **Directions**:
 * - **Vertical**: Positive = up/away, Negative = down/toward
 * - **Horizontal**: Positive = right, Negative = left
 *
 * @see kMsgDMouseWheel1_0
 * @since Protocol version 1.3
 */
extern const char *const kMsgDMouseWheel;

/**
 * @brief Mouse wheel scroll event (legacy v1.0-1.2)
 *
 * **Message Code**: `"DMWM"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DMWM%2i"`
 * **Parameters**:
 * - `$1`: Y delta (2 bytes, signed) - Vertical scroll only
 *
 * Legacy version that only supports vertical scrolling.
 * Used when communicating with protocol versions 1.0-1.2.
 *
 * @deprecated Use kMsgDMouseWheel for protocol version 1.3+
 * @see kMsgDMouseWheel
 * @since Protocol version 1.0
 */
extern const char *const kMsgDMouseWheel1_0;

/** @} */ // end of protocol_mouse group
