#pragma once

#include <tuple>
#include <vector>

#include "simeng/BranchPredictor.hh"
#include "simeng/Core.hh"
#include "simeng/Instruction.hh"
#include "simeng/MemoryInterface.hh"

namespace simeng {

using MacroOp = std::vector<std::shared_ptr<Instruction>>;

namespace arch {

enum class ChangeType { REPLACEMENT, INCREMENT, DECREMENT };

/** A structure describing a set of changes to the process state. */
struct ProcessStateChange {
  /** Type of changes to be made */
  ChangeType type;
  /** Registers to modify */
  std::vector<Register> modifiedRegisters;
  /** Values to set modified registers to */
  std::vector<RegisterValue> modifiedRegisterValues;
  /** Memory address/width pairs to modify */
  std::vector<MemoryAccessTarget> memoryAddresses;
  /** Values to write to memory */
  std::vector<RegisterValue> memoryAddressValues;
};

/** The result from a handled exception. */
struct ExceptionResult {
  /** Whether execution should halt. */
  bool fatal;
  /** The address to resume execution from. */
  uint64_t instructionAddress;
  /** Any changes to apply to the process state. */
  ProcessStateChange stateChange;
  /** The uop to forward onto dependant instructions. */
  std::shared_ptr<Instruction> uop;
};

/** An abstract multi-cycle exception handler interface. Should be ticked each
 * cycle until complete. */
class ExceptionHandler {
 public:
  virtual ~ExceptionHandler(){};
  /** Tick the exception handler to progress handling of the exception. Should
   * return `false` if the exception requires further handling, or `true` once
   * complete. */
  virtual bool tick() = 0;

  /** Retrieve the result of the exception. */
  virtual const ExceptionResult& getResult() const = 0;
};

/** An abstract Instruction Set Architecture (ISA) definition. Each supported
 * ISA should provide a derived implementation of this class. */
class Architecture {
 public:
  virtual ~Architecture(){};

  /** Attempt to pre-decode from `bytesAvailable` bytes of instruction memory.
   * Writes into the supplied macro-op vector, and returns the number of bytes
   * consumed to produce it; a value of 0 indicates too few bytes were present
   * for a valid decoding. */
  virtual uint8_t predecode(const void* ptr, uint8_t bytesAvailable,
                            uint64_t instructionAddress,
                            MacroOp& output) const = 0;

  /** Returns a vector of {size, number} pairs describing the available
   * registers. */
  virtual std::vector<RegisterFileStructure> getRegisterFileStructures()
      const = 0;

  /** Returns a zero-indexed register tag for a system register encoding. */
  virtual int32_t getSystemRegisterTag(uint16_t reg) const = 0;

  /** Returns the number of system registers that have a mapping. */
  virtual uint16_t getNumSystemRegisters() const = 0;

  /** Create an exception handler for the exception generated by
   * `instruction`, providing the core model object and a reference to
   * process memory. Returns a smart pointer to an `ExceptionHandler` which
   * may be ticked until the exception is resolved, and results then
   * obtained. */
  virtual std::shared_ptr<ExceptionHandler> handleException(
      const std::shared_ptr<Instruction>& instruction, const Core& core,
      MemoryInterface& memory) const = 0;

  /** Retrieve the initial process state. */
  virtual ProcessStateChange getInitialState() const = 0;

  /** Returns the maximum size of a valid instruction in bytes. */
  virtual uint8_t getMaxInstructionSize() const = 0;

  /** Returns the system register for the Virtual Counter Timer. */
  virtual simeng::Register getVCTreg() const = 0;

  /** Returns the system register for the Processor Cycle Counter. */
  virtual simeng::Register getPCCreg() const = 0;
};

}  // namespace arch
}  // namespace simeng
