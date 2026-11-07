/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2021 Barrier Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SecureUtilsTests.h"

#include "common/Constants.h"
#include "net/SecureUtils.h"

#include <QFile>
#include <QTemporaryDir>
#include <memory>
#include <openssl/pem.h>

using namespace synergy;

void SecureUtilsTests::checkHex()
{
  const QByteArray fingerprint(
      "\x28\xFD\x0A\x98\x8A\x0E\xA1\x6C\xD7\xE8\x6C\xA7\xEE\x58\x41\x71\xCA\xB2\x8E\x49\x25\x94\x90\x25\x26\x05\x8D\xAF"
      "\x63\xED\x2E\x30",
      32
  );
  QCOMPARE(
      synergy::formatSSLFingerprint(fingerprint, true),
      "28:FD:0A:98:8A:0E:A1:6C:D7:E8:6C:A7:EE:58:41:71:CA:B2:8E:49:25:94:90:25:26:05:8D:AF:63:ED:2E:30"
  );
}

void SecureUtilsTests::checkArt()
{
  const QByteArray fingerprint(
      "\x28\xFD\x0A\x98\x8A\x0E\xA1\x6C\xD7\xE8\x6C\xA7\xEE\x58\x41\x71\xCA\xB2\x8E\x49\x25\x94\x90\x25\x26\x05\x8D\xAF"
      "\x63\xED\x2E\x30",
      32
  );
  QCOMPARE(
      synergy::generateFingerprintArt(fingerprint), "╔═════════════════╗\n"
                                                    "║*X+. .           ║\n"
                                                    "║*oo +            ║\n"
                                                    "║ + =             ║\n"
                                                    "║  B  . .         ║\n"
                                                    "║.+... o S        ║\n"
                                                    "║E+ ++. .         ║\n"
                                                    "║B*++..  .        ║\n"
                                                    "║+o*o o .         ║\n"
                                                    "║+o*Bo .          ║\n"
                                                    "╚═════════════════╝"
  );
}

void SecureUtilsTests::generatedCertificateHasSelfSignedSubject()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath("synthetic-cert.pem");
  generatePemSelfSignedCert(path);
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const auto pem = file.readAll();
  const std::unique_ptr<BIO, decltype(&BIO_free)> input(BIO_new_mem_buf(pem.constData(), pem.size()), BIO_free);
  QVERIFY(input != nullptr);
  const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      PEM_read_bio_PrivateKey(input.get(), nullptr, nullptr, nullptr), EVP_PKEY_free
  );
  QVERIFY(key != nullptr);
  const std::unique_ptr<X509, decltype(&X509_free)> cert(
      PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr), X509_free
  );
  QVERIFY(cert != nullptr);
  const auto *subject = X509_get_subject_name(cert.get());
  const auto *issuer = X509_get_issuer_name(cert.get());
  QVERIFY(subject != nullptr);
  QVERIFY(issuer != nullptr);
  QCOMPARE(X509_NAME_cmp(subject, issuer), 0);
  char commonName[256] = {};
  QVERIFY(X509_NAME_get_text_by_NID(subject, NID_commonName, commonName, sizeof(commonName)) > 0);
  QCOMPARE(QByteArray(commonName), QByteArray(kAppName));
  QCOMPARE(X509_check_private_key(cert.get(), key.get()), 1);
  QCOMPARE(X509_verify(cert.get(), key.get()), 1);
  QVERIFY(EVP_PKEY_bits(key.get()) >= kMinimumRsaBits);
}

QTEST_MAIN(SecureUtilsTests)
