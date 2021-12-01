#include "simeng/Translator.hh"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace simeng {

typedef std::unordered_map<uint64_t, uint64_t> stringmap;

void Translator::enumerate_region(memoryRegion region_process,
                                  memoryRegion region_simulation, bool insert) {
  if (!disableTranslation_) {
    if (insert) {
      for (int i = 0; i < (region_process.addr_end - region_process.addr_start);
           i++) {
        mappings_[region_process.addr_start + i] =
            region_simulation.addr_start + i;
      }
    } else {
      for (int i = 0; i < (region_process.addr_end - region_process.addr_start);
           i++) {
        mappings_.erase({region_process.addr_start + i});
      }
    }
  }
}

Translator::Translator() {
  mappings_ = std::unordered_map<uint64_t, uint64_t>();
  regions_ = std::unordered_map<memoryRegion, memoryRegion, hash_fn>();
}

Translator::~Translator() {}

const Translation Translator::get_mapping(uint64_t addr) const {
  if (disableTranslation_) {
    return {addr, true};
  }
  // auto res = std::find_if(
  //     regions_.begin(), regions_.end(),
  //     [&](const std::pair<memoryRegion, memoryRegion>& mem) {
  //       return (addr >= mem.first.addr_start && addr < mem.first.addr_end);
  //     });
  // if (res != regions_.end()) {
  //   return {(addr - res->first.addr_start) + res->second.addr_start, true};
  // }
  // return {0, false};
  try {
    return {mappings_.at(addr), true};
  } catch (const std::out_of_range& e) {
    // std::cout << "FAILED to get mapping for 0x" << std::hex << addr <<
    // std::dec
    //           << std::endl;
    return {0, false};
  }
}

bool Translator::add_mapping(memoryRegion region_process,
                             memoryRegion region_simulation) {
  if (disableTranslation_) return true;
  // std::cout << "# New mapping:\n 0x" << std::hex << region_process.addr_start
  //           << std::dec << ":0x" << std::hex << region_process.addr_end
  //           << std::dec << " -> ";
  // Ensure translated block is the same size as the original
  if ((region_process.addr_end - region_process.addr_start) !=
      (region_simulation.addr_end - region_simulation.addr_start)) {
    // std::cout << "diff size ("
    //           << (region_process.addr_end - region_process.addr_start) << "
    //           vs "
    //           << (region_simulation.addr_end - region_simulation.addr_start)
    //           << ")" << std::endl;
    // assert(false && "Differently sized memory regions_");
    return false;
  }
  // std::min used to ensure boundaries compared against don't wrap around to
  // unsigned(-1)
  auto res =
      std::find_if(regions_.begin(), regions_.end(),
                   [&](const std::pair<memoryRegion, memoryRegion>& mem) {
                     return !(region_simulation.addr_start >
                                  std::min(mem.second.addr_end - 1, 0ull) ||
                              std::min(region_simulation.addr_end - 1, 0ull) <
                                  mem.second.addr_start);
                   });
  if (res != regions_.end()) {
    // std::cout << "Overlap:" << std::endl;
    // std::cout << "\t0x" << std::hex << res->second.addr_start << std::dec
    //           << " to 0x" << std::hex << res->second.addr_end << std::dec
    //           << std::endl;
    // std::cout << "\t0x" << std::hex << region_simulation.addr_start <<
    // std::dec
    //           << " to 0x" << std::hex << region_simulation.addr_end <<
    //           std::dec
    //           << std::endl;
    // assert(false && "Overlaps with previously allocated region");
    return false;
  }

  regions_.insert({region_process, region_simulation});
  enumerate_region(region_process, region_simulation, true);

  // std::cout << "0x" << std::hex << region_simulation.addr_start << std::dec
  //           << ":0x" << std::hex << region_simulation.addr_end << std::dec
  //           << std::endl;
  return true;
}

bool Translator::update_mapping(memoryRegion region_original,
                                memoryRegion region_process,
                                memoryRegion region_simulation) {
  if (disableTranslation_) return true;
  // std::cout << "# Update mapping:\n 0x" << std::hex
  //           << region_original.addr_start << std::dec << ":0x" << std::hex
  //           << region_original.addr_end << std::dec << "("
  //           << (region_original.addr_end - region_original.addr_start)
  //           << ") = 0x" << std::hex << region_process.addr_start << std::dec
  //           << ":0x" << std::hex << region_process.addr_end << std::dec <<
  //           "("
  //           << (region_process.addr_end - region_process.addr_start) << ") ->
  //           ";
  // Ensure translated block is the same size as the original
  if ((region_process.addr_end - region_process.addr_start) !=
      (region_simulation.addr_end - region_simulation.addr_start)) {
    // std::cout << "diff size ("
    //           << (region_process.addr_end - region_process.addr_start) << "
    //           vs "
    //           << (region_simulation.addr_end - region_simulation.addr_start)
    //           << ")" << std::endl;
    return false;
  }
  // Ensure old program region exists
  auto res_old = std::find_if(
      regions_.begin(), regions_.end(),
      [&](const std::pair<memoryRegion, memoryRegion>& mem) {
        return (region_original.addr_start == mem.first.addr_start &&
                region_original.addr_end == mem.first.addr_end);
      });
  if (res_old == regions_.end()) {
    // std::cout << "original doesn't exist" << std::endl;
    return false;
  }

  std::pair<memoryRegion, memoryRegion> temp = *res_old;
  regions_.erase(res_old);

  // std::cout << "Found entry 0x" << std::hex << temp.first.addr_start <<
  // std::dec
  //           << " to 0x" << std::hex << temp.first.addr_end << std::dec
  //           << " -> ";

  // Ensure new simeng region shares no boundary with previous region
  // std::min used to ensure boundaries compared against don't wrap around
  // to unsigned(-1)
  auto res =
      std::find_if(regions_.begin(), regions_.end(),
                   [&](const std::pair<memoryRegion, memoryRegion>& mem) {
                     return !(region_simulation.addr_start >
                                  std::min(mem.second.addr_end - 1, 0ull) ||
                              std::min(region_simulation.addr_end - 1, 0ull) <
                                  mem.second.addr_start);
                   });
  if (res != regions_.end()) {
    // std::cout << "overlaps prior region 0x" << std::hex
    //           << res->second.addr_start << std::dec << ":0x" << std::hex
    //           << res->second.addr_end << std::dec << " <- 0x" << std::hex
    //           << region_simulation.addr_start << std::dec << ":0x" <<
    //           std::hex
    //           << region_simulation.addr_end << std::dec << std::endl;
    regions_.insert(temp);
    return false;
  }
  // Add new mapping
  // add_mapping(b, c);
  regions_.insert({region_process, region_simulation});
  enumerate_region(region_process, region_simulation, true);
  // std::cout << "0x" << std::hex << temp.second.addr_start << std::dec <<
  // ":0x"
  //           << std::hex << temp.second.addr_end << std::dec << "("
  //           << (temp.second.addr_end - temp.second.addr_start) << ") = 0x"
  //           << std::hex << region_simulation.addr_start << std::dec << ":0x"
  //           << std::hex << region_simulation.addr_end << std::dec << "("
  //           << (region_simulation.addr_end - region_simulation.addr_start)
  //           << ")" << std::endl;

  return true;
}

uint64_t Translator::mmap_allocation(size_t length) {
  std::shared_ptr<struct heap_allocation> newAlloc(new heap_allocation);
  // Find suitable region to allocate
  memoryRegion previousAllocation = {0, 0};
  // Find if there's space between the first allocation and the set mmap region
  if (heapAllocations_.size()) {
    if ((heapAllocations_[0].start - programBrks_.first) > length) {
      newAlloc->start = programBrks_.first;
      newAlloc->mapped_region.addr_start = programBrks_.second;
      std::shared_ptr<heap_allocation> firstAlloc(&heapAllocations_[0]);
      newAlloc->next = firstAlloc;
    }
  }
  // Determine if the new allocation can fit between existing allocations
  if (newAlloc->start == 0) {
    for (heap_allocation& alloc : heapAllocations_) {
      if (alloc.next != NULL && (alloc.next->start - alloc.end) >= length) {
        newAlloc->start = alloc.end;
        newAlloc->mapped_region.addr_start = alloc.mapped_region.addr_end;
        // Re-link contiguous allocation to include new allocation
        newAlloc->next = alloc.next;
        alloc.next = newAlloc;
        break;
      }
    }
  }
  // If still not allocated, append allocation to end of list
  if (newAlloc->start == 0) {
    if (heapAllocations_.size()) {
      newAlloc->start = heapAllocations_.back().end;
      newAlloc->mapped_region.addr_start =
          heapAllocations_.back().mapped_region.addr_end;
      heapAllocations_.back().next = newAlloc;
    } else {
      // If no allocations exists, allocate to start of the simulation mmap
      // region
      newAlloc->start = programBrks_.first;
      newAlloc->mapped_region.addr_start = programBrks_.second;
    }
  }
  // Define end of regions_
  newAlloc->end = newAlloc->start + length;
  newAlloc->mapped_region.addr_end =
      newAlloc->mapped_region.addr_start + length;
  // The end of a mmap allocation must be rounded up to the nearest page size
  uint64_t remainder = (newAlloc->start + length) % pageSize_;
  newAlloc->end += (remainder != 0) ? (pageSize_ - remainder) : 0;
  newAlloc->mapped_region.addr_end +=
      (remainder != 0) ? (pageSize_ - remainder) : 0;
  heapAllocations_.push_back(*newAlloc);

  // std::cout << "Add Allocation:" << std::endl;
  // std::cout << "\t0x" << std::hex << newAlloc->start << std::dec << " to 0x "
  //           << std::hex << newAlloc->end << std::dec << std::endl;
  // std::cout << "\t0x" << std::hex << newAlloc->mapped_region.addr_start
  //           << std::dec << " to 0x" << std::hex
  //           << newAlloc->mapped_region.addr_end << std::dec << std::endl;

  // Add mapping for new mmap allocation
  add_mapping({newAlloc->start, newAlloc->end}, newAlloc->mapped_region);

  return newAlloc->start;
}

void Translator::register_allocation(uint64_t addr, size_t length,
                                     memoryRegion region_simulation) {
  // Create new heap_allocation that represents mmap allocation
  std::shared_ptr<struct heap_allocation> newAlloc(new heap_allocation);
  newAlloc->start = addr;
  newAlloc->end = addr + length;
  newAlloc->mapped_region = region_simulation;
  // Find where prior allocation would fit in current heapAllocations_
  for (heap_allocation& alloc : heapAllocations_) {
    if (alloc.start <= addr) {
      newAlloc->next = alloc.next;
      alloc.next = newAlloc;
      break;
    }
  }
  heapAllocations_.push_back(*newAlloc);
  // std::cout << "Register Allocation:" << std::endl;
  // std::cout << "\t0x" << std::hex << newAlloc->start << std::dec << " to 0x"
  //           << std::hex << newAlloc->end << std::dec << std::endl;
  // std::cout << "\t0x" << std::hex << newAlloc->mapped_region.addr_start
  //           << std::dec << " to 0x" << std::hex
  //           << newAlloc->mapped_region.addr_end << std::dec << std::endl;
}

int64_t Translator::munmap_deallocation(uint64_t addr, size_t length) {
  if (addr % pageSize_ != 0) {
    // addr must be a multiple of the process page size
    return -1;
  }

  heap_allocation alloc;
  // Find addr in allocations
  for (int i = 0; i < heapAllocations_.size(); i++) {
    alloc = heapAllocations_[i];
    if (alloc.start == addr) {
      if ((alloc.end - alloc.start) < length) {
        // length must not be larger than the original allocation as munmap
        // across multiple pages is not supported
        return -1;
      }
      // Fix next values to not include to-be erased entry
      if (i != 0) {
        heapAllocations_[i - 1].next = heapAllocations_[i].next;
      }
      // Erase mmap allocation
      heapAllocations_.erase(heapAllocations_.begin() + i);
      if (!disableTranslation_) {
        // Ensure program region exists in mappings_
        auto res_old = std::find_if(
            regions_.begin(), regions_.end(),
            [&](const std::pair<memoryRegion, memoryRegion>& mem) {
              return (alloc.mapped_region.addr_start == mem.second.addr_start &&
                      alloc.mapped_region.addr_end == mem.second.addr_end);
            });
        if (res_old == regions_.end()) {
          // std::cout << "original doesn't exist" << std::endl;
          return -1;
        }
        // Erase mapping entries
        regions_.erase(res_old->first);
        enumerate_region(res_old->first, res_old->second, false);
      }
      return 0;
    }
  }
  // Not an error if the indicated range does no contain any mapped pages
  return 0;
}

void Translator::setInitialBrk(uint64_t processAddress,
                               uint64_t simulationAddress) {
  programBrks_ = {processAddress, simulationAddress};
}

void Translator::setPageSize(uint64_t pagesize) { pageSize_ = pagesize; }

void Translator::disable_translation() { disableTranslation_ = true; }

}  // namespace simeng
