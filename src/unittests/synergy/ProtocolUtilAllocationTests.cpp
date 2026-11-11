/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ProtocolUtil.h"
#include "base/Log.h"
#include "io/IStream.h"
#include "synergy/core/SynergyException.h"

#include <QCoreApplication>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {
struct AllocationPolicy
{
  bool active = false;
  bool failArray = false;
  bool failDestination = false;
  bool destinationArmed = false;
  size_t size = 4096;
  unsigned injected = 0;
  unsigned arraysCreated = 0;
  void *array = nullptr;
};
thread_local AllocationPolicy policy;

void *allocate(size_t size)
{
  if (void *result = std::malloc(size == 0 ? 1 : size))
    return result;
  throw std::bad_alloc();
}
} // namespace

// Isolated executable only: the production allocator and other tests are unchanged.
void *operator new(size_t size)
{
  if (policy.active && policy.failDestination && policy.destinationArmed && size >= policy.size) {
    policy.failDestination = false;
    ++policy.injected;
    throw std::bad_alloc();
  }
  return allocate(size);
}

void *operator new[](size_t size)
{
  if (policy.active && size == policy.size && policy.failArray) {
    policy.failArray = false;
    ++policy.injected;
    throw std::bad_alloc();
  }
  void *result = allocate(size);
  if (policy.active && size == policy.size) {
    policy.array = result;
    ++policy.arraysCreated;
  }
  return result;
}

void operator delete(void *pointer) noexcept
{
  std::free(pointer);
}
void operator delete(void *pointer, size_t) noexcept
{
  std::free(pointer);
}
void operator delete[](void *pointer) noexcept
{
  if (pointer == policy.array)
    policy.array = nullptr;
  std::free(pointer);
}
void operator delete[](void *pointer, size_t) noexcept
{
  ::operator delete[](pointer);
}

namespace {
class ReadStream : public synergy::IStream
{
public:
  explicit ReadStream(const std::string &wire) : m_wire(wire)
  {
  }
  uint32_t read(void *buffer, uint32_t count) override
  {
    const auto take = (std::min)(static_cast<size_t>(count), m_wire.size() - consumed);
    std::memcpy(buffer, m_wire.data() + consumed, take);
    consumed += take;
    if (consumed == m_wire.size())
      policy.destinationArmed = true;
    return static_cast<uint32_t>(take);
  }
  void close() override
  {
  }
  void write(const void *, uint32_t) override
  {
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
    return const_cast<ReadStream *>(this);
  }
  bool isReady() const override
  {
    return consumed < m_wire.size();
  }
  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(m_wire.size() - consumed);
  }
  size_t consumed = 0;

private:
  const std::string &m_wire;
};

bool runFailure(bool failArray)
{
  const std::string wire = std::string("\0\0\x10\0", 4) + std::string(4096, '\x91');
  ReadStream stream(wire);
  std::string destination = "unchanged sentinel";
  policy = {};
  policy.failArray = failArray;
  policy.failDestination = !failArray;
  policy.active = true;
  const bool result = ProtocolUtil::readf(&stream, "%s", &destination);
  policy.active = false;
  const bool noTemporaryLeak = policy.array == nullptr;
  const bool correct = !result && destination == "unchanged sentinel" && policy.injected == 1 && noTemporaryLeak &&
                       (failArray ? stream.consumed == 4 && policy.arraysCreated == 0
                                  : stream.consumed == wire.size() && policy.arraysCreated == 1);
  std::printf(
      "%s: %s injected=%u outstanding_array=%u\n",
      failArray ? "temporary allocation failure" : "destination allocation failure", correct ? "PASS" : "FAIL",
      policy.injected, noTemporaryLeak ? 0 : 1
  );
  // Preserve fail-first evidence without intentionally leaking in the test process.
  if (policy.array) {
    std::free(policy.array);
    policy.array = nullptr;
  }
  return correct;
}

bool repeatedTruncation()
{
  const std::string wire = std::string("\0\0\x10\0", 4) + std::string(129, '\x91');
  for (unsigned i = 0; i < 1000; ++i) {
    ReadStream stream(wire);
    std::string destination = "unchanged sentinel";
    policy = {};
    policy.active = true;
    const bool result = ProtocolUtil::readf(&stream, "%s", &destination);
    policy.active = false;
    if (result || destination != "unchanged sentinel" || policy.array || policy.arraysCreated != 1 ||
        stream.consumed != wire.size())
      return false;
  }
  std::puts("1000 truncated heap-buffer reads: PASS outstanding_array=0");
  return true;
}

bool oversizedBeforeAllocation()
{
  const std::string wire("\0\x01\0\x01", 4); // 65537: one byte above the unchanged reader limit.
  ReadStream stream(wire);
  std::string destination = "unchanged sentinel";
  policy = {};
  policy.size = 65537;
  policy.failArray = true;
  policy.active = true;
  bool rejected = false;
  try {
    ProtocolUtil::readf(&stream, "%s", &destination);
  } catch (const BadClientException &) {
    rejected = true;
  }
  policy.active = false;
  const bool correct = rejected && policy.injected == 0 && policy.arraysCreated == 0 && !policy.array &&
                       stream.consumed == 4 && destination == "unchanged sentinel";
  std::printf(
      "oversize before allocation: %s injected=%u arrays=%u consumed=%zu\n", correct ? "PASS" : "FAIL", policy.injected,
      policy.arraysCreated, stream.consumed
  );
  return correct;
}
} // namespace

int main(int argc, char **argv)
{
  QCoreApplication application(argc, argv);
  Log log;
  log.setFilter(LogLevel::Level::Error);
  const bool temporary = runFailure(true);
  const bool destination = runFailure(false);
  const bool truncation = repeatedTruncation();
  const bool oversize = oversizedBeforeAllocation();
  return temporary && destination && truncation && oversize ? EXIT_SUCCESS : EXIT_FAILURE;
}
