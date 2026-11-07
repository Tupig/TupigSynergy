/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
/**
 * @addtogroup protocol_data
 * @{
 */

/**
 * @defgroup protocol_clipboard Clipboard Messages
 * @brief Clipboard data synchronization messages
 * @{
 */

/**
 * @brief Clipboard data transfer
 *
 * **Message Code**: `"DCLP"`
 * **Direction**: Primary ↔ Secondary
 * **Format**: `"DCLP%1i%4i%1i%s"`
 * **Parameters**:
 * - `$1`: Clipboard identifier (1 byte)
 * - `$2`: Sequence number (4 bytes)
 * - `$3`: Mark/flags (1 byte) - For streaming support (v1.6+)
 * - `$4`: Clipboard data (string)
 *
 * **Example**:
 *
 * Primary clipboard, sequence 1, no flags, text "Hello World"
 * ```
 * "DCLP\x00\x00\x00\x00\x01\x00\x00\x00\x00\x0BHello World"
 * ```
 *
 * **Clipboard Identifiers**:
 * - `0`: Primary clipboard (Ctrl+C/Ctrl+V)
 * - `1`: Selection clipboard (middle-click on X11)
 *
 * **Sequence Numbers**:
 * - Primary always sends sequence number 0
 * - Secondary uses sequence number from most recent kMsgCEnter
 *
 * **Streaming (v1.6+)**:
 * For large clipboard data, the mark byte enables chunked transfer:
 * - `0`: Single chunk (complete data)
 * - `1`: First chunk of multi-chunk transfer
 * - `2`: Middle chunk
 * - `3`: Final chunk
 *
 * @see kMsgCClipboard
 * @since Protocol version 1.0
 */
extern const char *const kMsgDClipboard;

/** @} */ // end of protocol_clipboard group

/**
 * @defgroup protocol_info Information Messages
 * @brief Screen information and configuration messages
 * @{
 */

/**
 * @brief Client screen information
 *
 * **Message Code**: `"DINF"`
 * **Direction**: Secondary → Primary
 * **Format**: `"DINF%2i%2i%2i%2i%2i%2i%2i"`
 * **Parameters**:
 * - `$1`: Left edge coordinate (2 bytes, signed)
 * - `$2`: Top edge coordinate (2 bytes, signed)
 * - `$3`: Screen width in pixels (2 bytes, unsigned)
 * - `$4`: Screen height in pixels (2 bytes, unsigned)
 * - `$5`: Warp zone size (2 bytes, obsolete)
 * - `$6`: Mouse X position (2 bytes, signed)
 * - `$7`: Mouse Y position (2 bytes, signed)
 *
 * **Example**:
 *
 * Screen at (0,0), 1920x1080, mouse at (400,300)
 * ```
 * "DINF\x00\x00\x00\x00\x07\x80\x04\x38\x00\x00\x01\x90\x01\x2C"
 * ```
 *
 * **When to Send**:
 * 1. In response to kMsgQInfo query
 * 2. When screen resolution changes
 * 3. During initial connection setup
 *
 * **Resolution Change Protocol**:
 * When sending due to resolution change, the secondary should:
 * 1. Send kMsgDInfo with new dimensions
 * 2. Ignore kMsgDMouseMove until receiving kMsgCInfoAck
 * 3. This prevents mouse movement outside the new screen area
 *
 * @see kMsgQInfo, kMsgCInfoAck
 * @since Protocol version 1.0
 */
extern const char *const kMsgDInfo;

/**
 * @brief Set client options
 *
 * **Message Code**: `"DSOP"`
 * **Direction**: Primary → Secondary
 * **Format**: `"DSOP%4I"`
 * **Parameters**:
 * - `$1`: Option/value pairs (4-byte integer list)
 *
 * **Example**:
 *
 * ```
 * "DSOP\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x00"
 * 2 pairs: option 1 = value 1, option 2 = value 0
 * ```
 *
 * Sets configuration options on the client. Options are sent as
 * alternating option ID and value pairs. The client should apply
 * these settings to modify its behavior.
 *
 * @see kMsgCResetOptions
 * @since Protocol version 1.0
 */
extern const char *const kMsgDSetOptions;

/** @} */ // end of protocol_info group

/**
 * @defgroup protocol_files File Transfer Messages
 * @brief File transfer and drag-and-drop messages (v1.5+)
 * @{
 */

/**
 * @brief File transfer data
 *
 * **Message Code**: `"DFTR"`
 * **Direction**: Primary ↔ Secondary
 * **Format**: `"DFTR%1i%s"`
 * **Parameters**:
 * - `$1`: Transfer mark (1 byte) - Transfer state
 * - `$2`: Data (string) - Content depends on mark
 *
 * **Transfer Marks**:
 * - `1` (kDataStart): Data is the file size, as a **decimal string** (so `4096`
 *   arrives as the four bytes `"4096"`). Not an 8-byte integer - the `%s` field
 *   carries text, and FileChunk::start encodes it with std::to_string while
 *   FileChunk::assemble parses it with std::from_chars.
 * - `2` (kDataChunk): Data contains file content chunk
 * - `3` (kDataEnd): Transfer complete (data may be empty)
 *
 * Each chunk payload must fit the `%s` transport ceiling
 * (PROTOCOL_MAX_STRING_LENGTH). Oversized chunks are not merely slow:
 * ProtocolUtil::readBytes() throws BadClientException, which the dispatchers
 * treat as a protocol error and drop the connection.
 *
 * **Example Transfer Sequence**:
 *
 * Send 4096 bytes
 * ```
 * "DFTR\x01" "4096"      # mark=1, declared size as decimal text
 * "DFTR\x02[16384 bytes of file data]"
 * "DFTR\x02[16384 bytes of file data]"
 * "DFTR\x03"             # mark=3, empty payload
 * ```
 * (A `%s` payload is length-prefixed by ProtocolUtil; the quotes above delimit it
 * for readability only.)
 *
 * **Protocol Flow**:
 * 1. Sender initiates with kDataStart containing total file size
 * 2. Sender sends multiple kDataChunk messages with file content
 * 3. Sender concludes with kDataEnd to signal completion
 * 4. Receiver can abort by closing connection
 *
 * @see kMsgDDragInfo, EDataTransfer
 * @since Protocol version 1.5
 * @deprecated File drag and drop is no longer implemented.
 */
extern const char *const kMsgDFileTransfer;

/**
 * @brief Drag and drop information
 *
 * **Message Code**: `"DDRG"`
 * **Direction**: Primary ↔ Secondary
 * **Format**: `"DDRG%2i%s"`
 * **Parameters**:
 * - `$1`: Number of files (2 bytes)
 * - `$2`: File paths (string) - Null-separated file paths
 *
 * **Example**:
 *
 * Dragging 2 files
 * ``` * "DDRG\x00\x02/path/to/file1.txt\x00/path/to/file2.txt\x00"
 * ```
 *
 * Sent when a drag-and-drop operation begins. Contains the list
 * of files being dragged. The actual file transfer follows using
 * kMsgDFileTransfer messages.
 *
 * **File Path Format**:
 * - Paths are null-terminated strings
 * - Multiple paths are concatenated with null separators
 * - Paths should use forward slashes for compatibility
 *
 * @see kMsgDFileTransfer
 * @since Protocol version 1.5
 * @deprecated File drag and drop is no longer implemented.
 */
extern const char *const kMsgDDragInfo;

/** @} */ // end of protocol_files group

/**
 * @defgroup protocol_system System Messages
 * @brief System-level notifications and synchronization
 * @{
 */

/**
 * @brief Secure input notification (macOS)
 *
 * **Message Code**: `"SECN"`
 * **Direction**: Primary → Secondary
 * **Format**: `"SECN%s"`
 * **Parameters**:
 * - `$1`: Application name (string) - App requesting secure input
 *
 * **Example**:
 *
 * Terminal app is requesting secure input
 * ```
 * "SECN\x00\x00\x00\x08Terminal"
 * ```
 *
 * Notifies the secondary screen when an application on the primary
 * requests secure input mode. This is primarily a macOS feature
 * where certain applications (like password fields) can request
 * exclusive keyboard access.
 *
 * The secondary can use this information to:
 * - Display security warnings to the user
 * - Temporarily disable input forwarding
 * - Show which application is requesting secure input
 *
 * @since Protocol version 1.7
 */
extern const char *const kMsgDSecureInputNotification;

/**
 * @brief Language synchronization
 *
 * **Message Code**: `"LSYN"`
 * **Direction**: Primary → Secondary
 * **Format**: `"LSYN%s"`
 * **Parameters**:
 * - `$1`: Language list (string) - Available server languages
 *
 * **Example**:
 *
 * Server supports English, French, German, Spanish
 * ```
 * "LSYN\x00\x00\x00\x0Ben,fr,de,es"
 * ```
 *
 * Synchronizes keyboard language/layout information between
 * primary and secondary screens. Helps ensure proper character
 * mapping when different keyboard layouts are used.
 *
 * **Language Format**:
 * - Comma-separated list of language codes
 * - Uses standard ISO 639-1 language codes
 * - Primary language listed first
 *
 * @since Protocol version 1.8
 */
extern const char *const kMsgDLanguageSynchronisation;

/** @} */ // end of protocol_system group

/** @} */ // end of protocol_data group
