#pragma once

#include "simeng/MemoryInterface.hh"

#include <unordered_map>
#include <vector>

#include <sst/core/interfaces/simpleMem.h>

using namespace SST::Interfaces;

namespace simeng {

/** An SST memory interface request. */
struct SSTMemoryInterfaceRequest {

  /** Is this request split since it straddles cache lines? **/
  bool split;

  /** If this request has been split, has it's partner been handled? **/
  bool partnerHandled;

  /** The memory target to access. */
  const MemoryAccessTarget target;

  /** The value to write to the target (writes only) */
  const RegisterValue data;

  /** A unique request identifier for read operations. */
  uint64_t requestId;

  uint64_t partnerSSTId;

  std::vector<uint8_t> splitData;

  /** Construct a write request. */
  SSTMemoryInterfaceRequest(const MemoryAccessTarget &target,
                            const RegisterValue &data)
      : partnerHandled(false), split(false), target(target), data(data) {}

  /** Construct a split write request. */
  SSTMemoryInterfaceRequest(const MemoryAccessTarget &target,
                            const RegisterValue &data, uint64_t partnerSSTId)
      : partnerHandled(false), split(true), target(target), data(data),
        partnerSSTId(partnerSSTId) {}

  /** Construct a read request. */
  SSTMemoryInterfaceRequest(const MemoryAccessTarget &target,
                            uint64_t requestId)
      : partnerHandled(false), split(false), target(target),
        requestId(requestId) {}

  /** Construct a split read request. */
  SSTMemoryInterfaceRequest(const MemoryAccessTarget &target,
                            uint64_t requestId, uint64_t partnerSSTId)
      : partnerHandled(false), split(true), target(target),
        requestId(requestId), partnerSSTId(partnerSSTId) {}
};

/** An SST memory interface. */
class SSTMemoryInterface : public MemoryInterface {
public:
  SSTMemoryInterface(char *memory, size_t size, SimpleMem *mem,
                     uint64_t cacheLineWidth);

  /** Queue a read request from the supplied target location.
   *
   * The caller can optionally provide an ID that will be attached to completed
   * read results.
   */
  void requestRead(const MemoryAccessTarget &target,
                   uint64_t requestId = 0) override;
  /** Queue a write request of `data` to the target location. */
  void requestWrite(const MemoryAccessTarget &target,
                    const RegisterValue &data) override;
  /** Retrieve all completed requests. */
  const span<MemoryReadResult> getCompletedReads() const override;

  /** Clear the completed reads. */
  void clearCompletedReads() override;

  /** Returns true if there are any oustanding memory requests in-flight. */
  bool hasPendingRequests() const override;

  /** Tick the memory model to process the request queue. */
  void tick() override;

  void handleResponse(bool read, uint64_t id, std::vector<uint8_t> data);

private:
  /** The SST memory interface. */
  SimpleMem *mem_;
  /** The array representing the memory system to access. */
  char *memory_;
  /** The size of accessible memory. */
  size_t size_;
  /** A vector containing all completed read requests. */
  std::vector<MemoryReadResult> completedReads_;

  /** A queue containing all pending memory requests. */
  std::unordered_map<int, SSTMemoryInterfaceRequest> pendingRequests_;

  /** The number of times this interface has been ticked. */
  uint64_t tickCounter_ = 0;

  uint64_t cacheLineWidth_;
};

} // namespace simeng
