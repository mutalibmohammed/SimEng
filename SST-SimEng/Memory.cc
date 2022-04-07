#include "Memory.hh"

#include <sst/core/interfaces/simpleMem.h>

namespace simeng {

uint64_t sendReadRequest(SimpleMem* mem, uint64_t address, uint8_t size) {
  SimpleMem::Request* req =
      new SimpleMem::Request(SimpleMem::Request::Read, address, size);
  mem->sendRequest(req);
  return req->id;
}

uint64_t sendWriteRequest(SimpleMem* mem, uint64_t address, uint8_t size,
                          std::vector<uint8_t> payload) {
  SimpleMem::Request* req =
      new SimpleMem::Request(SimpleMem::Request::Write, address, size);
  req->setPayload(payload);
  mem->sendRequest(req);
  return req->id;
}

SSTMemoryInterface::SSTMemoryInterface(char* memory, size_t size,
                                       SimpleMem* mem, uint64_t cacheLineWidth)
    : memory_(memory),
      size_(size),
      mem_(mem),
      cacheLineWidth_(cacheLineWidth) {}

void SSTMemoryInterface::tick() { tickCounter_++; }

void SSTMemoryInterface::requestRead(const MemoryAccessTarget& target,
                                     uint64_t requestId) {
  if (target.address + target.size > size_) {
    // Read outside of memory; return an invalid value to signal a fault
    completedReads_.push_back({target, RegisterValue(), requestId});
  } else {
    uint64_t readStart = target.address % cacheLineWidth_;
    uint64_t readEnd = readStart + target.size;
    if (readEnd > cacheLineWidth_) {
      uint8_t firstSize = cacheLineWidth_ - readStart;
      uint8_t secondSize = target.size - firstSize;
      uint64_t secondAddress = target.address + firstSize;
      // Split
      auto id1 = sendReadRequest(mem_, target.address, firstSize);
      auto id2 = sendReadRequest(mem_, secondAddress, secondSize);
      pendingRequests_.insert({id1, {target, requestId, id2}});
      pendingRequests_.insert({id2, {target, requestId, id1}});
    } else {
      // Normal read
      auto id = sendReadRequest(mem_, target.address, target.size);
      pendingRequests_.insert({id, {target, requestId}});
    }
  }
}

void SSTMemoryInterface::requestWrite(const MemoryAccessTarget& target,
                                      const RegisterValue& data) {
  assert(target.address + target.size <= size_ &&
         "Attempted to write beyond memory limit");
  uint64_t writeStart = target.address % cacheLineWidth_;
  uint64_t writeEnd = writeStart + target.size;
  if (writeEnd > cacheLineWidth_) {
    uint8_t firstSize = cacheLineWidth_ - writeStart;
    uint8_t secondSize = target.size - firstSize;
    uint64_t secondAddress = target.address + firstSize;
    // Split
    std::vector<uint8_t> payload1(firstSize, 0);
    std::vector<uint8_t> payload2(secondSize, 0);
    memcpy((void*)&payload1[0], data.getAsVector<char>(), firstSize);
    memcpy((void*)&payload2[0], &(data.getAsVector<char>()[firstSize]),
           secondSize);
    auto id1 = sendWriteRequest(mem_, target.address, firstSize, payload1);
    auto id2 = sendWriteRequest(mem_, secondAddress, secondSize, payload2);
    pendingRequests_.insert({id1, {target, data, id2}});
    pendingRequests_.insert({id2, {target, data, id1}});
  } else {
    std::vector<uint8_t> payload(target.size, 0);
    memcpy((void*)&payload[0], data.getAsVector<char>(), target.size);
    auto id = sendWriteRequest(mem_, target.address, target.size, payload);
    pendingRequests_.insert({id, {target, data}});
  }
}

const span<MemoryReadResult> SSTMemoryInterface::getCompletedReads() const {
  return {const_cast<MemoryReadResult*>(completedReads_.data()),
          completedReads_.size()};
}

void SSTMemoryInterface::clearCompletedReads() { completedReads_.clear(); }

bool SSTMemoryInterface::hasPendingRequests() const {
  return !pendingRequests_.empty();
}

std::vector<uint8_t> mergeData(bool firstId, std::vector<uint8_t> d1,
                               std::vector<uint8_t> d2) {
  std::vector<uint8_t> data;
  if (firstId) {
    d1.insert(d1.end(), d2.begin(), d2.end());
    data = d1;
  } else {
    d2.insert(d2.end(), d1.begin(), d1.end());
    data = d2;
  }
  return data;
}

// Call this in the event handler.
void SSTMemoryInterface::handleResponse(bool read, uint64_t id,
                                        std::vector<uint8_t> data) {
  auto& request = pendingRequests_.at(id);

  if (read) {
    if (request.split) {
      if (request.partnerHandled) {
        // Both requests are finished.
        auto mergedData =
            mergeData(id < request.partnerSSTId, data, request.splitData);
        const char* char_data = reinterpret_cast<const char*>(&mergedData[0]);
        completedReads_.push_back(
            {request.target, RegisterValue(char_data, request.target.size),
             request.requestId});
        pendingRequests_.erase(id);
        pendingRequests_.erase(request.partnerSSTId);
      } else {
        pendingRequests_.at(request.partnerSSTId).splitData = data;
        pendingRequests_.at(request.partnerSSTId).partnerHandled = true;
      }
    } else {
      // SimEng uses char and SST uses uint8_t.
      const char* char_data = reinterpret_cast<const char*>(&data[0]);
      completedReads_.push_back({request.target,
                                 RegisterValue(char_data, request.target.size),
                                 request.requestId});
      pendingRequests_.erase(id);
    }
  } else {
    if (request.split) {
      if (request.partnerHandled) {
        // Both requests are finished.
        pendingRequests_.erase(id);
        pendingRequests_.erase(request.partnerSSTId);
      } else {
        pendingRequests_.at(request.partnerSSTId).partnerHandled = true;
      }
    } else {
      pendingRequests_.erase(id);
    }
  }
}

}  // namespace simeng