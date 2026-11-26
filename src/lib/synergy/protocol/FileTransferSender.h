/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "FileTransferSource.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

//! Drives a whole outbound file transfer: announce, then one file at a time.
/*!
Splits the job in two so each half stays testable on its own. This class decides
*when* to emit and what to say; FileTransferSource owns reading local files. The
wire encoding itself lives in FileChunk, so there is exactly one place that knows
the `DFTR` layout.

The order is not negotiable: the peer learns where each file goes from the `DDRG`
announcement, so it must arrive before any content. Files are then sent in the
order announced, because the protocol carries no name in the content messages - the
receiver pairs the Nth completed transfer with the Nth announced name.

Chunking is FileChunk::chunkSize(), which is derived from the transport ceiling.
Exceeding it is not a slow path: ProtocolUtil rejects an over-long `%s` with
BadClientException and the dispatchers drop the connection.
*/
class FileTransferSender
{
public:
  //! Where the produced messages go.
  /*!
  Deliberately not a stream: the caller owns transmission, which keeps this class
  free of the network, the event queue and the platform layer.
  */
  struct Emitter
  {
    //! Called once, before any content, with the file count and NUL-separated names.
    std::function<void(uint32_t fileCount, const std::string &names)> dragInfo;

    //! Called once per content message: a ChunkType mark plus its payload.
    std::function<void(uint8_t mark, const std::string &payload)> chunk;
  };

  //! Announce \p entries and stream them in their announced order.
  /*!
  Once announced, each entry owns one positional slot at the peer. If an entry
  cannot be opened, or it becomes shorter while it is read, transmission stops
  so a later file cannot be stored under the failed entry's name. A started frame
  is still closed with an end chunk before stopping. Bytes beyond the size
  declared at the start of a frame are never sent.
  \param entries files to send, in the order they were announced
  \param emitter receives the messages, in order
  \return content bytes from complete frames streamed before any failure
  */
  static uint64_t send(const std::vector<FileTransferSource::Entry> &entries, const Emitter &emitter);
};
