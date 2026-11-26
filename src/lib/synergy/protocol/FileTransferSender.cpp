/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FileTransferSender.h"

#include "ProtocolTypes.h"
#include "base/Log.h"

uint64_t FileTransferSender::send(const std::vector<FileTransferSource::Entry> &entries, const Emitter &emitter)
{
  // An empty drag would promise the peer zero files and then send nothing.
  if (entries.empty()) {
    return 0;
  }

  if (!emitter.dragInfo || !emitter.chunk) {
    LOG_ERR(QT_TRANSLATE_NOOP("EngineLogMessages", "cannot send a file transfer without both emitter callbacks"));
    return 0;
  }

  if (entries.size() > 0xFFFF) {
    LOG_ERR("cannot send a file transfer: %zu files exceeds the 16-bit count field", entries.size());
    return 0;
  }

  emitter.dragInfo(static_cast<uint32_t>(entries.size()), FileTransferSource::dragInfoPayload(entries));

  uint64_t total = 0;
  size_t completedFiles = 0;
  FileTransferSource source;

  for (auto entry : entries) {
    // open() refreshes the size from the handle, so the declared size is the one
    // the bytes actually come from. A file edited since inspect() would otherwise
    // be announced with a stale size and the peer would reject the whole transfer.
    if (source.open(entry) != FileTransferSource::Status::Ok) {
      LOG_WARN("stopping file transfer: \"%s\" could not be opened for sending", entry.path.c_str());
      break;
    }

    // Every started file must be closed with an end chunk, or the peer is left
    // mid-transfer and refuses the next start as a restart while active.
    emitter.chunk(static_cast<uint8_t>(ChunkType::DataStart), std::to_string(entry.size));

    uint64_t remaining = entry.size;
    uint64_t fileBytes = 0;
    while (remaining != 0) {
      auto piece = source.readChunk();
      if (piece.empty()) {
        break;
      }

      if (piece.size() > remaining) {
        piece.resize(static_cast<size_t>(remaining));
      }

      emitter.chunk(static_cast<uint8_t>(ChunkType::DataChunk), piece);
      fileBytes += piece.size();
      remaining -= piece.size();
    }

    emitter.chunk(static_cast<uint8_t>(ChunkType::DataEnd), std::string());
    source.close();

    if (remaining != 0) {
      LOG_WARN(
          "stopping file transfer: \"%s\" ended with %llu of %llu byte(s)", entry.path.c_str(),
          static_cast<unsigned long long>(fileBytes), static_cast<unsigned long long>(entry.size)
      );
      break;
    }

    total += fileBytes;
    ++completedFiles;
  }

  LOG_DEBUG("sent %zu file(s), %llu byte(s) of content", completedFiles, static_cast<unsigned long long>(total));
  return total;
}
