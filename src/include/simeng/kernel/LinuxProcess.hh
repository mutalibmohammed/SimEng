#pragma once

#include <memory>

#include "simeng/Elf.hh"
#include "yaml-cpp/yaml.h"

namespace simeng {
namespace kernel {

/** Align `address` to an `alignTo`-byte boundary by rounding up to the nearest
 * multiple. */
uint64_t alignToBoundary(uint64_t value, uint64_t boundary);

/** The initial state of a Linux process, constructed from a binary executable.
 *
 * The constructed process follows a typical layout:
 *
 * |---------------| <- start of stack
 * |     Stack     |    stack grows downwards
 * |-v-----------v-|
 * |               |
 * |-^-----------^-|
 * |  mmap region  |    mmap region grows upwards
 * |---------------| <- start of mmap region
 * |               |
 * |-^-----------^-|
 * |     Heap      |    heap grows upwards
 * |---------------| <- start of heap
 * |               |
 * |  ELF-defined  |
 * | process image |
 * |               |
 * |---------------| <- 0x0
 *
 */
class LinuxProcess {
 public:
  /** Construct a Linux process from a vector of command-line arguments.
   *
   * The first argument is a path to an executable ELF file. */
  LinuxProcess(const std::vector<std::string>& commandLine, YAML::Node config);

  /** Construct a Linux process from region of instruction memory, with the
   * entry point fixed at 0. */
  LinuxProcess(span<char> instructions, YAML::Node config);

  ~LinuxProcess();

  /** Get the address of the start of the heap region. */
  uint64_t getHeapStart() const;

  /** Get the address of the top of the stack. */
  uint64_t getStackStart() const;

  /** Get the address of the start of the mmap region. */
  uint64_t getMmapStart() const;

  /** Get the page size. */
  uint64_t getPageSize() const;

  /** Get a shared_ptr to process image. */
  std::shared_ptr<char> getProcessImage() const;

  /** Get the size of the process image. */
  uint64_t getProcessImageSize() const;

  /** Get the entry point. */
  uint64_t getEntryPoint() const;

  /** Get the initial stack pointer address. */
  uint64_t getStackPointer() const;

  /** Get the path of the executable. */
  std::string getPath() const;

  /** Check whether the process image was created successfully. */
  bool isValid() const;

 private:
  /** The size of the stack, in bytes. */
  const uint64_t STACK_SIZE;

  /** The space to reserve for the heap, in bytes. */
  const uint64_t HEAP_SIZE;

  /** Create and populate the initial process stack. */
  void createStack(char** processImage);

  /** The entry point of the process. */
  uint64_t entryPoint_ = 0;

  /** The address of the start of the heap region. */
  uint64_t heapStart_;

  /** The address of the start of region of memory given to mmap. */
  uint64_t mmapStart_;

  /** The page size of the process memory. */
  const uint64_t pageSize_ = 4096;

  /** The address of the stack pointer. */
  uint64_t stackPointer_;

  /** The process image size. */
  uint64_t size_;

  /** The process command and its arguments. */
  std::vector<std::string> commandLine_;

  /** Whether the process image was created successfully. */
  bool isValid_ = false;

  /** Shared pointer to processImage. */
  std::shared_ptr<char> processImage_;
};

}  // namespace kernel
}  // namespace simeng
