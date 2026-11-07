/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

#include "ProtocolControlMessages.h"
#include "ProtocolDataMessages.h"
#include "ProtocolInputMessages.h"
/**
 * @file ProtocolTypes.h
 * @brief Synergy Network Protocol Specification and Implementation
 *
 * @section protocol_overview Protocol Overview
 *
 * This file defines the data types, constants, and message structures for the
 * Synergy network protocol. For a high-level conceptual overview of the protocol,
 * including the distinction between Client/Server and Primary/Secondary roles,
 * please see the @ref protocol_reference "Protocol Reference" document.
 */

/**
 * @defgroup protocol_constants Protocol Constants
 * @brief Core protocol version and configuration constants
 * @{
 */

/**
 * @brief Protocol major version number
 *
 * The major version indicates fundamental protocol compatibility.
 * Clients and servers with different major versions cannot communicate.
 *
 * @since Protocol version 1.0
 */
inline constexpr int16_t kProtocolMajorVersion = 1;

/**
 * @brief Protocol minor version number
 *
 * The minor version indicates feature availability within the same major version.
 * Higher minor versions are backward compatible with lower minor versions.
 *
 * @note When incrementing the minor version, the Synergy application version should also increment
 * @since Protocol version 1.0
 */
inline constexpr int16_t kProtocolMinorVersion = 8;

/**
 * @brief Default TCP port for Synergy connections
 *
 * Clients connect to this port on the server by default.
 * Can be overridden in configuration.
 *
 * @since Protocol version 1.0
 */
inline constexpr uint16_t kDefaultPort = 24800;

/**
 * @brief Maximum length for client greeting message
 *
 * Limits the size of the hello message sent by clients during handshake.
 * Prevents memory exhaustion attacks during connection establishment.
 *
 * @since Protocol version 1.0
 */
inline constexpr uint32_t kMaxHelloLength = 1024;

/**
 * @brief Keep-alive message interval in seconds
 *
 * Time between kMsgCKeepAlive messages sent by the server.
 * A non-positive value disables keep-alives.
 * This is the default rate that can be overridden using options.
 *
 * @see kMsgCKeepAlive
 * @since Protocol version 1.3
 */
static const double kKeepAliveRate = 3.0;

/**
 * @brief Keep-alive timeout threshold
 *
 * Number of consecutive missed kMsgCKeepAlive messages that indicates
 * a connection problem. After this many missed messages, the connection
 * is considered dead and will be terminated.
 *
 * @see kMsgCKeepAlive
 * @since Protocol version 1.3
 */
static const double kKeepAlivesUntilDeath = 3.0;

/**
 * @brief Obsolete heartbeat rate (deprecated)
 *
 * @deprecated Since protocol version 1.3, replaced by keep-alive mechanism
 * @see kKeepAliveRate
 */
static const double kHeartRate = -1.0;

/**
 * @brief Obsolete heartbeat timeout (deprecated)
 *
 * @deprecated Since protocol version 1.3, replaced by keep-alive mechanism
 * @see kKeepAlivesUntilDeath
 */
static const double kHeartBeatsUntilDeath = 3.0;

/**
 * @defgroup protocol_size_limits Message Size Limits
 * @brief Tiered size limits for different message categories
 * @{
 */

/**
 * @brief Tiered message size limits
 *
 * Different message types have different maximum allowed sizes based on their
 * content. This prevents memory exhaustion attacks while allowing legitimate
 * large payloads (e.g., clipboard data, file transfers).
 *
 * @since Protocol version 1.8
 */
enum class MessageSizeLimit : uint32_t
{
  /**
   * @brief Control messages (handshake, keep-alive, screen enter/leave)
   *
   * These are small fixed-format messages. Anything larger indicates
   * a protocol error or malicious client.
   */
  Control = 256,

  /**
   * @brief Input events (keyboard, mouse, clipboard metadata)
   *
   * Input events are small but may include language strings or
   * option lists. Generous limit for future extensibility.
   */
  InputEvent = 4 * 1024,

  /**
   * @brief Clipboard data chunks
   *
   * Clipboard data is transmitted in chunks. Each chunk should not
   * exceed this limit. Full clipboard content may span multiple chunks.
   */
  ClipboardChunk = 64 * 1024,

  /**
   * @brief File transfer data chunks
   *
   * File transfer data is transmitted in chunks. Each chunk should not
   * exceed this limit.
   */
  FileChunk = 256 * 1024,

  /**
   * @brief Maximum total message size
   *
   * Absolute upper bound for any single protocol message. Messages
   * exceeding this are always rejected, regardless of type.
   */
  AbsoluteMaximum = 4 * 1024 * 1024
};

/** @} */ // end of protocol_size_limits group

/**
 * @brief Maximum allowed message length
 *
 * @deprecated Use MessageSizeLimit::AbsoluteMaximum for new code.
 * Messages exceeding this size indicate a likely protocol error.
 * Such messages are not parsed and cause connection termination.
 * This prevents memory exhaustion attacks.
 *
 * @note Clipboard and file chunks using the %s format specifier are separately
 * limited to PROTOCOL_MAX_STRING_LENGTH (= MessageSizeLimit::ClipboardChunk) by
 * ProtocolUtil::readBytes(). A larger chunk is rejected as a protocol error,
 * which drops the connection rather than degrading gracefully, so senders must
 * stay within it (see StreamChunker).
 * @since Protocol version 1.0
 */
static constexpr uint32_t PROTOCOL_MAX_MESSAGE_LENGTH = static_cast<uint32_t>(MessageSizeLimit::AbsoluteMaximum);

/**
 * @brief Maximum allowed list length in protocol messages
 *
 * @deprecated Use MessageSizeLimit::InputEvent for new code.
 * Limits the size of lists (arrays) in protocol messages to prevent
 * memory exhaustion attacks.
 *
 * @since Protocol version 1.0
 */
static constexpr uint32_t PROTOCOL_MAX_LIST_LENGTH = static_cast<uint32_t>(MessageSizeLimit::InputEvent);

/**
 * @brief Maximum allowed string length in protocol messages
 *
 * @deprecated Use MessageSizeLimit::ClipboardChunk for new code.
 * Limits the size of strings in protocol messages to prevent
 * memory exhaustion attacks.
 *
 * @since Protocol version 1.0
 */
static constexpr uint32_t PROTOCOL_MAX_STRING_LENGTH = static_cast<uint32_t>(MessageSizeLimit::ClipboardChunk);

/** @} */ // end of protocol_constants group

/**
 * @defgroup protocol_enums Protocol Enumerations
 * @brief Enumeration types used in protocol messages
 * @{
 */

/**
 * @brief File transfer data chunk types
 *
 * Used in kMsgDFileTransfer messages to indicate the type of data
 * being transferred in each chunk. Used in clipboard operations.
 *
 * @since Protocol version 1.5
 */
struct ChunkType
{
  inline static const auto DataStart = 1; ///< Start of transfer (contains file size)
  inline static const auto DataChunk = 2; ///< Data chunk (contains file content)
  inline static const auto DataEnd = 3;   ///< End of transfer (transfer complete)
};

/**
 * @brief Data reception state codes
 *
 * Used internally to track the state of data reception
 * during clipboard operations.
 *
 * @since Protocol version 1.5
 */
enum class TransferState : uint8_t
{
  Started,    ///< Reception started
  InProgress, ///< Reception in progress
  Finished,   ///< Reception completed successfully
  Error       ///< Reception failed with error
};

/** @} */ // end of protocol_enums group

/**
 * @defgroup protocol_structures Protocol Data Structures
 * @brief Data structures used in protocol messages
 * @{
 */

/**
 * @brief Client screen information structure
 *
 * Contains comprehensive information about a secondary screen,
 * including dimensions, position, and current mouse location.
 * This data is sent via kMsgDInfo messages.
 *
 * **Usage**:
 * - Sent by client in response to kMsgQInfo
 * - Sent when screen resolution changes
 * - Used by server for screen layout calculations
 *
 * **Coordinate System**:
 * - Origin (0,0) is typically top-left corner
 * - Coordinates can be negative for multi-monitor setups
 * - All values are in pixels
 *
 * @see kMsgDInfo, kMsgQInfo
 * @since Protocol version 1.0
 */
class ClientInfo
{
public:
  /**
   * @brief Screen position coordinates
   *
   * The position of the upper-left corner of the screen in the
   * virtual desktop coordinate system. This is typically (0,0)
   * for single-monitor setups, but can be different in multi-monitor
   * configurations.
   *
   * @since Protocol version 1.0
   */
  int32_t m_x; ///< Left edge X coordinate
  int32_t m_y; ///< Top edge Y coordinate

  /**
   * @brief Screen dimensions
   *
   * The size of the screen in pixels. These values represent
   * the usable screen area for mouse movement and window placement.
   *
   * @since Protocol version 1.0
   */
  int32_t m_w; ///< Screen width in pixels
  int32_t m_h; ///< Screen height in pixels

  /**
   * @brief Obsolete jump zone size
   *
   * @deprecated This field is no longer used and should be set to 0
   * @since Protocol version 1.0
   */
  int32_t obsolete1;

  /**
   * @brief Current mouse position
   *
   * The current location of the mouse cursor on this screen.
   * Coordinates are relative to the screen's coordinate system
   * (m_x, m_y represent the origin).
   *
   * **Usage**:
   * - Updated when mouse moves on this screen
   * - Used for cursor synchronization between screens
   * - Helps with smooth transitions during screen switching
   *
   * @since Protocol version 1.0
   */
  int32_t m_mx; ///< Mouse X position
  int32_t m_my; ///< Mouse Y position
};

/** @} */ // end of protocol_structures group
