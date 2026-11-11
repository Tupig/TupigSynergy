/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 - 2016 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "Chunk.h"
#include "ClipboardTypes.h"
#include "ProtocolTypes.h"

#include <cstddef>
#include <memory>
#include <string>

constexpr static auto s_clipboardChunkMetaSize = 7;

namespace synergy {
class IStream;
}

struct ClipboardChunkAssemblyState
{
  size_t expectedSize = 0;
  bool active = false;
};

class ClipboardChunk : public Chunk
{
public:
  // Internal event ownership only; this identity is never serialized.
  struct Lifetime
  {
  };

  explicit ClipboardChunk(size_t size, const std::shared_ptr<const Lifetime> &lifetime = {});

  static ClipboardChunk *start(
      ClipboardID id, uint32_t sequence, const std::string &size, const std::shared_ptr<const Lifetime> &lifetime = {}
  );
  static ClipboardChunk *data(
      ClipboardID id, uint32_t sequence, const std::string &data, const std::shared_ptr<const Lifetime> &lifetime = {}
  );
  static ClipboardChunk *end(ClipboardID id, uint32_t sequence, const std::shared_ptr<const Lifetime> &lifetime = {});

  bool belongsTo(const std::shared_ptr<const Lifetime> &lifetime) const
  {
    return lifetime && m_lifetime == lifetime;
  }

  static TransferState assemble(
      synergy::IStream *stream, std::string &dataCached, ClipboardID &id, uint32_t &sequence,
      ClipboardChunkAssemblyState &state, size_t maxDataSize
  );

  static void send(synergy::IStream *stream, void *data);

  static size_t getExpectedSize(const ClipboardChunkAssemblyState &state)
  {
    return state.expectedSize;
  }

private:
  std::shared_ptr<const Lifetime> m_lifetime;
};
