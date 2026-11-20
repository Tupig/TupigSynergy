/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QHostAddress>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

//! Minimal loopback HTTP server for update checker/downloader tests.
class TestHttpServer : public QTcpServer
{
public:
  explicit TestHttpServer(QObject *parent = nullptr) : QTcpServer(parent)
  {
    listen(QHostAddress::LocalHost, 0);
  }

  QUrl url(const QString &path = QStringLiteral("/")) const
  {
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));
  }

  void setJson(const QByteArray &body, int status = 200)
  {
    m_status = status;
    m_body = body;
    m_contentType = QByteArrayLiteral("application/json");
    m_redirect.clear();
  }

  void setBinary(const QByteArray &body, const QByteArray &contentType = QByteArrayLiteral("application/octet-stream"))
  {
    m_status = 200;
    m_body = body;
    m_contentType = contentType;
    m_redirect.clear();
  }

  void setRedirect(const QUrl &location)
  {
    m_redirect = location;
  }

  void setByteRanges(bool enabled, bool incorrect = false)
  {
    m_ranges = enabled;
    m_incorrectRange = incorrect;
  }

  int partialRequests() const
  {
    return m_partialRequests;
  }
  int ordinaryRequests() const
  {
    return m_ordinaryRequests;
  }

protected:
  void incomingConnection(qintptr handle) override
  {
    auto *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(handle);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket, request = QByteArray()]() mutable {
      request += socket->readAll();
      if (!request.contains("\r\n\r\n"))
        return;
      socket->disconnect(this);
      static const QRegularExpression range(
          QStringLiteral("\\r\\nRange: bytes=(\\d+)-(\\d+)\\r\\n"), QRegularExpression::CaseInsensitiveOption
      );
      const auto match = range.match(QString::fromLatin1(request));
      if (match.hasMatch())
        ++m_partialRequests;
      else
        ++m_ordinaryRequests;
      const auto reason = m_status == 200 ? QByteArrayLiteral("OK") : QByteArrayLiteral("Error");
      QByteArray response;
      if (!m_redirect.isEmpty()) {
        response = "HTTP/1.1 302 Found\r\nLocation: " + m_redirect.toEncoded() +
                   "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      } else if (m_ranges && match.hasMatch()) {
        const auto start = match.captured(1).toLongLong();
        const auto end = match.captured(2).toLongLong();
        const auto data = m_body.mid(start, end - start + 1);
        response = "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes " + QByteArray::number(start) + "-" +
                   QByteArray::number(end) + "/" + QByteArray::number(m_body.size() + m_incorrectRange) +
                   "\r\nContent-Length: " + QByteArray::number(data.size()) + "\r\nConnection: close\r\n\r\n" + data;
      } else {
        response = "HTTP/1.1 " + QByteArray::number(m_status) + " " + reason + "\r\nContent-Type: " + m_contentType +
                   "\r\nContent-Length: " + QByteArray::number(m_body.size()) + "\r\nConnection: close\r\n\r\n" +
                   m_body;
      }
      socket->write(response);
      socket->disconnectFromHost();
    });
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
  }

private:
  int m_status = 200;
  QByteArray m_body;
  QByteArray m_contentType = QByteArrayLiteral("application/json");
  QUrl m_redirect;
  bool m_ranges = false;
  bool m_incorrectRange = false;
  int m_partialRequests = 0;
  int m_ordinaryRequests = 0;
};
