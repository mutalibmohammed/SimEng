#ifndef __H_INSTRUCTION
#define __H_INSTRUCTION

#include "registerFile.hh"
#include "registerValue.hh"

#include <vector>

typedef short InstructionException;

namespace simeng {

/** An abstract instruction definition.
 * Each supported ISA should provide a derived implementation of this class. */
class Instruction {
 public:
  virtual ~Instruction(){};

  /** Retrieve the identifier for the first exception that occurred during
   * decoding or execution. */
  virtual InstructionException getException() const = 0;

  /** Retrieve a vector of source registers this instruction reads. */
  virtual const std::vector<Register> &getOperandRegisters() const = 0;

  /** Retrieve a vector of destination registers this instruction will write to.
   * A register value of -1 signifies a Zero Register read, and should not be
   * renamed. */
  virtual const std::vector<Register> &getDestinationRegisters() const = 0;

  /** Override the destination and operand registers with renamed physical
   * register tags. */
  virtual void rename(const std::vector<Register> &destinations,
                      const std::vector<Register> &operands) = 0;

  /** Provide a value for the specified physical register. */
  virtual void supplyOperand(const Register &reg,
                             const RegisterValue &value) = 0;

  /** Check whether the operand at index `i` has had a value supplied. */
  virtual bool isOperandReady(int i) const = 0;

  /** Check whether all operand values have been supplied, and the instruction
   * is ready to execute. */
  virtual bool canExecute() const = 0;

  /** Execute the instruction. */
  virtual void execute() = 0;

  /** Check whether the instruction has executed and has results ready to
   * commit. */
  virtual bool canCommit() const = 0;

  /** Retrieve register results to commit. */
  virtual std::vector<RegisterValue> getResults() const = 0;

  /** Generate memory addresses this instruction wishes to access. */
  virtual std::vector<std::pair<uint64_t, uint8_t>> generateAddresses() = 0;

  /** Provide data from a requested memory address. */
  virtual void supplyData(uint64_t address, const RegisterValue &data) = 0;

  /** Retrieve previously generated memory addresses. */
  virtual std::vector<std::pair<uint64_t, uint8_t>> getGeneratedAddresses() const = 0;

  /** Retrieve supplied memory data. */
  virtual std::vector<RegisterValue> getData() const = 0;

  /** Check for misprediction. */
  virtual bool wasBranchMispredicted() const = 0;

  /** Retrieve branch address. */
  virtual uint64_t getBranchAddress() const = 0;

  /** Is this a store operation? */
  virtual bool isStore() const = 0;

  /** Is this a load operation? */
  virtual bool isLoad() const = 0;

  /** Is this a branch operation? */
  virtual bool isBranch() const = 0;
};

}  // namespace simeng

#endif