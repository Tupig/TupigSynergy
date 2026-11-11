/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ProtocolUtil.h"
#include "base/Log.h"
#include "io/IStream.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef SYNERGY_BENCHMARK_COMPILER
#define SYNERGY_BENCHMARK_COMPILER "unrecorded"
#endif
#ifndef SYNERGY_BENCHMARK_FLAGS
#define SYNERGY_BENCHMARK_FLAGS "unrecorded"
#endif
#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

namespace {
class OutputSink : public synergy::IStream
{
public:
  explicit OutputSink(size_t capacity) : bytes(capacity)
  {
  }
  BENCH_NOINLINE void write(const void *buffer, uint32_t size) override
  {
    if (size > bytes.size())
      std::abort();
    std::memcpy(bytes.data(), buffer, size);
    lastSize = size;
    bytesTotal += size;
    ++calls;
  }
  void close() override
  {
  }
  uint32_t read(void *, uint32_t) override
  {
    return 0;
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<OutputSink *>(this);
  }
  bool isReady() const override
  {
    return false;
  }
  uint32_t getSize() const override
  {
    return 0;
  }
  std::vector<uint8_t> bytes;
  uint32_t lastSize = 0;
  uint64_t bytesTotal = 0;
  uint64_t calls = 0;
};

void emitRecord(const QJsonObject &record)
{
  const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
  std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
}
} // namespace

int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addOptions({
      {{"c", "case"}, "string or raw", "case", "string"},
      {{"n", "bytes"}, "Payload size (0,32,4096,262144)", "bytes", "4096"},
      {{"i", "iterations"}, "Frozen iterations per warm batch; 0 calibrates baseline", "iterations", "0"},
      {"batches", "Number of warm raw samples", "batches", "30"},
      {"variant", "baseline or candidate", "variant", "baseline"},
      {"run-id", "Measurement run identity", "run-id"},
      {"source-sha", "Verified source commit", "source-sha"},
      {"source-file-sha256", "Verified production ProtocolUtil source hash", "source-file-sha256"},
      {"build-manifest-sha256", "Verified compiler/link/dependency manifest hash", "build-manifest-sha256"},
      {"pair-index", "Alternating process pair", "pair-index", "0"},
  });
  parser.process(application);
  const bool raw = parser.value("case") == "raw";
  const auto size = parser.value("bytes").toUInt();
  uint64_t iterations = parser.value("iterations").toULongLong();
  const unsigned batches = parser.value("batches").toUInt();
  if ((parser.value("case") != "string" && !raw) || (size != 0 && size != 32 && size != 4096 && size != 262144) ||
      batches == 0 || batches > 1000)
    return EXIT_FAILURE;

  Log log;
  log.setFilter(LogLevel::Level::Error);
  std::string payload(size, '\0');
  for (size_t i = 0; i < payload.size(); ++i)
    payload[i] = static_cast<char>((i * 131 + 17) & 0xff);
  if (size > 0)
    payload[0] = '\0';
  if (size > 1)
    payload[1] = static_cast<char>(0xff);
  std::vector<uint8_t> expected = {
      static_cast<uint8_t>(size >> 24), static_cast<uint8_t>(size >> 16), static_cast<uint8_t>(size >> 8),
      static_cast<uint8_t>(size)
  };
  expected.insert(expected.end(), payload.begin(), payload.end());
  OutputSink sink(expected.size());
  const uint8_t sentinel = 0;
  const auto *data = size == 0 ? &sentinel : reinterpret_cast<const uint8_t *>(payload.data());
  auto invoke = [&] {
    if (raw)
      ProtocolUtil::writef(&sink, "%S", size, data);
    else
      ProtocolUtil::writef(&sink, "%s", &payload);
  };
  auto measure = [&](uint64_t count) {
    sink.calls = sink.bytesTotal = 0;
    const auto start = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < count; ++i)
      invoke();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  };
  auto valid = [&](uint64_t count) {
    return sink.calls == count && sink.bytesTotal == count * expected.size() && sink.lastSize == expected.size() &&
           std::equal(expected.begin(), expected.end(), sink.bytes.begin());
  };
  const auto cold = measure(1);
  if (!valid(1))
    return EXIT_FAILURE;
  const QString checksum = QString::fromLatin1(
      QCryptographicHash::hash(
          QByteArray(reinterpret_cast<const char *>(sink.bytes.data()), static_cast<int>(sink.lastSize)),
          QCryptographicHash::Sha256
      )
          .toHex()
  );
  QJsonObject context{
      {"kind", "context"},
      {"run_id", parser.value("run-id")},
      {"variant", parser.value("variant")},
      {"source_sha", parser.value("source-sha")},
      {"source_file_sha256", parser.value("source-file-sha256")},
      {"build_manifest_sha256", parser.value("build-manifest-sha256")},
      {"compiler", SYNERGY_BENCHMARK_COMPILER},
      {"flags", SYNERGY_BENCHMARK_FLAGS},
      {"dependency_versions", QJsonObject{{"qt", qVersion()}}},
      {"os", QSysInfo::prettyProductName()},
      {"arch", QSysInfo::currentCpuArchitecture()},
      {"case", parser.value("case")},
      {"payload_bytes", static_cast<int>(size)},
      {"pair_index", parser.value("pair-index").toInt()},
      {"output_sha256", checksum},
      {"tracked_allocations", QJsonValue::Null},
      {"tracked_bytes", QJsonValue::Null}
  };
  emitRecord(context);
  auto sample = [&](const char *state, unsigned batch, uint64_t count, qint64 elapsed) {
    QJsonObject row = context;
    row["kind"] = "sample";
    row["warm_state"] = state;
    row["batch_index"] = static_cast<int>(batch);
    row["iterations"] = static_cast<qint64>(count);
    row["total_ns"] = elapsed;
    row["ns_per_call"] = static_cast<double>(elapsed) / static_cast<double>(count);
    row["bytes_total"] = static_cast<qint64>(sink.bytesTotal);
    emitRecord(row);
  };
  sample("cold", 0, 1, cold);
  for (unsigned i = 0; i < 1000; ++i)
    invoke();
  if (iterations == 0) {
    if (parser.value("variant") != "baseline")
      return EXIT_FAILURE;
    iterations = 1;
    while (measure(iterations) < 100000000) {
      if (iterations > (uint64_t{1} << 40))
        return EXIT_FAILURE;
      iterations *= 2;
    }
  }
  for (unsigned batch = 0; batch < batches; ++batch) {
    const auto elapsed = measure(iterations);
    if (!valid(iterations))
      return EXIT_FAILURE;
    sample("warm", batch, iterations, elapsed);
  }
  return EXIT_SUCCESS;
}
