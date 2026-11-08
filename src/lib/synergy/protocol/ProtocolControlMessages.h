/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
/**
 * @defgroup protocol_messages Protocol Message Definitions
 * @brief All protocol message types and their format specifications
 *
 * In parameter descriptions, `$n` refers to the n\'th argument (counting from one).
 *
 * Messages are categorized by their first letter:
 * - **Greeting**: Connection handshake messages (no prefix)
 * - **C**: Command messages (server → client control)
 * - **D**: Data messages (input events, clipboard, files)
 * - **Q**: Query messages (information requests)
 * - **E**: Error messages (protocol violations, failures)
 *
 * @{
 */

/**
 * @defgroup protocol_handshake Handshake Messages
 * @brief Connection establishment and version negotiation
 * @{
 */

/**
 * @brief Server hello message
 *
 * **Direction**: Primary → Secondary
 * **Format**: `"%7s%2i%2i"`
 * **Parameters**:
 * - `$1`: Protocol name (7 bytes, fixed-size) - A fixed-size field for the protocol
 *   identifier, used for backward compatibility. The supported values are "Synergy" or
 *   "Barrier". "Barrier" is the default protocol name, while "Synergy" is used only
 *   for backwards compatibility. This is an exception to the standard length-prefixed string format.
 * - `$2`: Server major version number (2 bytes)
 * - `$3`: Server minor version number (2 bytes)
 *
 * **Example**:
 *
 *  Barrier protocol, version 1.8
 * ```
 * "Barrier\x00\x01\x00\x08"
 * ```
 *
 * This is the first message sent by the server after a client connects.
 * The client uses this to determine protocol compatibility.
 *
 * @see kMsgHelloBack
 * @since Protocol version 1.0
 */
extern const char *const kMsgHello;

/**
 * @brief Format string for server hello message arguments
 *
 * **Format**: `"%2i%2i"`
 * **Parameters**:
 * - `$1`: Server major version number (2 bytes)
 * - `$2`: Server minor version number (2 bytes)
 *
 * Used as part of dynamic hello message construction.
 *
 * @see kMsgHello
 * @since Protocol version 1.0
 */
extern const char *const kMsgHelloArgs;

/**
 * @brief Client hello response message
 *
 * **Direction**: Secondary → Primary
 * **Format**: `"%7s%2i%2i%s"`
 * **Parameters**:
 * - `$1`: Protocol name (7 bytes, fixed-size) - A fixed-size field for the protocol
 *   identifier, which must match the server's. The supported values are "Synergy" or
 *   "Barrier". "Barrier" is the default protocol name, while "Synergy" is used only
 *   for backwards compatibility. This is an exception to the standard length-prefixed string format.
 * - `$2`: Client major version number (2 bytes)
 * - `$3`: Client minor version number (2 bytes)
 * - `$4`: Client name (string) - A standard length-prefixed string.
 *
 * **Example**:
 *
 * Barrier protocol, version 1.8, client name "workstation"
 * ```
 * "Barrier\x00\x01\x00\x08\x00\x00\x00\x0Bworkstation"
 * ```
 *
 * Sent by the client in response to kMsgHello. After this exchange,
 * both sides negotiate to use the minimum supported protocol version.
 *
 * @see kMsgHello
 * @since Protocol version 1.0
 */
extern const char *const kMsgHelloBack;

/**
 * @brief Format string for client hello response arguments
 *
 * **Format**: `"%2i%2i%s"`
 * **Parameters**:
 * - `$1`: Client major version number (2 bytes)
 * - `$2`: Client minor version number (2 bytes)
 * - `$3`: Client name (string)
 *
 * Used as part of dynamic hello message construction.
 *
 * @see kMsgHelloBack
 * @since Protocol version 1.0
 */
extern const char *const kMsgHelloBackArgs;

/** @} */ // end of protocol_handshake group

/**
 * @defgroup protocol_commands Command Messages
 * @brief Control messages for screen management and connection maintenance
 * @{
 */

/**
 * @brief No operation command
 *
 * **Message Code**: `"CNOP"`
 * **Direction**: Secondary → Primary
 * **Format**: No parameters
 *
 * A no-operation message that can be used for testing connectivity
 * or as a placeholder. Has no effect on the receiving end.
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgCNoop;

/**
 * @brief Close connection command
 *
 * **Message Code**: `"CBYE"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Instructs the client to close the connection gracefully.
 * The client should clean up resources and disconnect.
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgCClose;

/**
 * @brief Enter screen command
 *
 * **Message Code**: `"CINN"`
 * **Direction**: Primary → Secondary
 * **Format**: `"CINN%2i%2i%4i%2i"`
 * **Parameters**:
 * - `$1`: Entry X coordinate (2 bytes, signed)
 * - `$2`: Entry Y coordinate (2 bytes, signed)
 * - `$3`: Sequence number (4 bytes, unsigned)
 * - `$4`: Modifier key mask (2 bytes, unsigned)
 *
 * **Example**:
 *
 * Enter at (400, 300), sequence 1, no modifiers
 * ```
 * "CINN\x01\x90\x01\x2C\x00\x00\x00\x01\x00\x00"
 * ```
 *
 * Sent when the mouse cursor enters the secondary screen from the primary.
 * The coordinates specify the exact entry point. The sequence number is used
 * to order messages and must be returned in subsequent messages from the client.
 * The modifier mask indicates which toggle keys (Caps Lock, Num Lock, etc.)
 * are active and should be synchronized on the secondary screen.
 *
 * @see kMsgCLeave
 * @since Protocol version 1.0
 */
extern const char *const kMsgCEnter;

/**
 * @brief Leave screen command
 *
 * **Message Code**: `"COUT"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Sent when the mouse cursor leaves the secondary screen and returns to the primary.
 * Upon receiving this message, the secondary screen should:
 * 1. Send clipboard data for any clipboards it has grabbed
 * 2. Only send clipboards that have changed since the last leave
 * 3. Use the sequence number from the most recent kMsgCEnter
 *
 * @see kMsgCEnter, kMsgCClipboard
 * @since Protocol version 1.0
 */
extern const char *const kMsgCLeave;

/**
 * @brief Clipboard grab notification
 *
 * **Message Code**: `"CCLP"`
 * **Direction**: Primary ↔ Secondary
 * **Format**: `"CCLP%1i%4i"`
 * **Parameters**:
 * - `$1`: Clipboard identifier (1 byte)
 * - `$2`: Sequence number (4 bytes)
 *
 * **Example**:
 *
 * Primary clipboard grabbed, sequence 1
 * ```
 * "CCLP\x00\x00\x00\x00\x01"
 * ```
 *
 * Sent when an application grabs a clipboard on either screen.
 * This notifies the other screen that clipboard ownership has changed.
 * Secondary screens must use the sequence number from the most recent
 * kMsgCEnter. The primary always sends sequence number 0.
 *
 * **Clipboard Identifiers**:
 * - `0`: Primary clipboard (Ctrl+C/Ctrl+V)
 * - `1`: Selection clipboard (middle-click on X11)
 *
 * @see kMsgDClipboard
 * @since Protocol version 1.0
 */
extern const char *const kMsgCClipboard;

/**
 * @brief Screensaver state change
 *
 * **Message Code**: `"CSEC"`
 * **Direction**: Primary → Secondary
 * **Format**: `"CSEC%1i"`
 * **Parameters**:
 * - `$1`: Screensaver state (1 byte): 1 = started, 0 = stopped
 * **Examples**:
 *
 * Screensaver started
 * ```
 * "CSEC\x01"
 * ```
 *
 * Screensaver stopped
 * ```
 * "CSEC\x00"
 * ```
 *
 * Notifies the secondary screen when the primary's screensaver
 * starts or stops. The secondary can use this to synchronize
 * its own screensaver state.
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgCScreenSaver;

/**
 * @brief Reset options command
 *
 * **Message Code**: `"CROP"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Instructs the client to reset all of its options to their default values.
 * This is typically sent when the server configuration changes.
 *
 * @see kMsgDSetOptions
 * @since Protocol version 1.0
 */
extern const char *const kMsgCResetOptions;

/**
 * @brief Screen information acknowledgment
 *
 * **Message Code**: `"CIAK"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Sent by the primary in response to a secondary screen's kMsgDInfo message.
 * This acknowledgment is sent for every kMsgDInfo, whether or not the
 * primary had previously sent a kMsgQInfo query.
 *
 * @see kMsgDInfo, kMsgQInfo
 * @since Protocol version 1.0
 */
extern const char *const kMsgCInfoAck;

/**
 * @brief Keep-alive message
 *
 * **Message Code**: `"CALV"`
 * **Direction**: Primary ↔ Secondary
 * **Format**: No parameters
 *
 * Sent periodically by the server to verify that connections are still
 * active. Clients must reply with the same message upon receipt.
 *
 * **Timing**:
 * - Default interval: 3 seconds (configurable)
 * - Timeout: 3 missed messages trigger disconnection
 *
 * **Behavior**:
 * - Server sends keep-alive to client
 * - Client immediately responds with keep-alive back to server
 * - If server doesn't receive response within timeout, it disconnects client
 * - If client doesn't receive keep-alives, it should disconnect from server
 *
 * @see kKeepAliveRate, kKeepAlivesUntilDeath
 * @since Protocol version 1.3
 */
extern const char *const kMsgCKeepAlive;

/** @} */ // end of protocol_commands group

/**
 * @defgroup protocol_queries Query Messages
 * @brief Information request messages
 * @{
 */

/**
 * @brief Query screen information
 *
 * **Message Code**: `"QINF"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Requests the secondary screen to send its current screen information.
 * The client should respond with a kMsgDInfo message containing:
 * - Screen dimensions and position
 * - Current mouse position
 * - Other screen-related data
 *
 * This is typically sent:
 * - During initial connection setup
 * - When the server needs updated screen information
 * - After configuration changes
 *
 * @see kMsgDInfo, kMsgCInfoAck
 * @since Protocol version 1.0
 */
extern const char *const kMsgQInfo;

/** @} */ // end of protocol_queries group

/**
 * @defgroup protocol_errors Error Messages
 * @brief Protocol error and failure notifications
 * @{
 */

/**
 * @brief Incompatible protocol versions
 *
 * **Message Code**: `"EICV"`
 * **Direction**: Primary → Secondary
 * **Format**: `"EICV%2i%2i"`
 * **Parameters**:
 * - `$1`: Primary major version (2 bytes)
 * - `$2`: Primary minor version (2 bytes)
 *
 * **Example**:
 *
 * Server is version 1.8
 * ```
 * "EICV\x00\x01\x00\x08"
 * ```
 *
 * Sent when the client and server have incompatible protocol versions.
 * This typically occurs when:
 * - Major versions differ (fundamental incompatibility)
 * - Server requires newer features than client supports
 *
 * After sending this message, the server will disconnect the client.
 * The client should display an appropriate error message to the user.
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgEIncompatible;

/**
 * @brief Client name already in use
 *
 * **Message Code**: `"EBSY"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Sent when the client name provided during connection is already
 * in use by another connected client. Client names must be unique
 * within a Synergy network.
 *
 * After receiving this message, the client should:
 * 1. Disconnect from the server
 * 2. Inform the user of the name conflict
 * 3. Allow the user to choose a different name
 * 4. Retry connection with the new name
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgEBusy;

/**
 * @brief Unknown client name
 *
 * **Message Code**: `"EUNK"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Sent when the client name provided during connection is not
 * found in the server's screen configuration map. This means
 * the server doesn't know about this client.
 *
 * This can happen when:
 * - Client name is misspelled
 * - Server configuration doesn't include this client
 * - Client is connecting to wrong server
 *
 * The client should inform the user and check:
 * - Client name spelling
 * - Server configuration
 * - Network connectivity to correct server
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgEUnknown;

/**
 * @brief Protocol violation
 *
 * **Message Code**: `"EBAD"`
 * **Direction**: Primary → Secondary
 * **Format**: No parameters
 *
 * Sent when the client violates the protocol in some way.
 * This can include:
 * - Sending malformed messages
 * - Sending messages in wrong order
 * - Sending unexpected message types
 * - Exceeding protocol limits
 *
 * After sending this message, the server will immediately
 * disconnect the client. This indicates a serious protocol
 * error that cannot be recovered from.
 *
 * **Common Causes**:
 * - Implementation bugs in client
 * - Corrupted network data
 * - Version mismatch not caught earlier
 * - Malicious or broken client
 *
 * @since Protocol version 1.0
 */
extern const char *const kMsgEBad;

/** @} */ // end of protocol_errors group

/** @} */ // end of protocol_messages group
