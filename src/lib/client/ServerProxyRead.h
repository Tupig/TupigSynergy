/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "ProtocolUtil.h"
#include "client/IServerProxyClient.h"
#include "client/ServerProxy.h"

template <typename... Args> bool ServerProxy::readOrDisconnect(const char *format, Args &&...args)
{
  if (ProtocolUtil::readf(m_stream, format, args...)) {
    return true;
  }
  // readf returns false when the stream ends mid-message, leaving the output
  // arguments partially written; never forward those values
  m_client->disconnect("truncated message");
  return false;
}
