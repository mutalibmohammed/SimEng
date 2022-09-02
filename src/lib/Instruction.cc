#include "simeng/Instruction.hh"

namespace simeng {

bool Instruction::exceptionEncountered() const { return exceptionEncountered_; }

void Instruction::setInstructionAddress(uint64_t address) {
  instructionAddress_ = address;
}
uint64_t Instruction::getInstructionAddress() const {
  return instructionAddress_;
}

void Instruction::setBranchPrediction(BranchPrediction prediction) {
  prediction_ = prediction;
}

BranchPrediction Instruction::getBranchPrediction() const {
  return prediction_;
}

uint64_t Instruction::getBranchAddress() const { return branchAddress_; }
bool Instruction::wasBranchTaken() const { return branchTaken_; }

bool Instruction::wasBranchMispredicted() const {
  assert(executed_ &&
         "Branch misprediction check requires instruction to have executed");

  // Flag as mispredicted if taken state was wrongly predicted, or taken and
  // predicted target is wrong
  return (branchTaken_ != prediction_.taken ||
          (branchTaken_ && prediction_.target != branchAddress_));
}

void Instruction::setSequenceId(uint64_t seqId) { sequenceId_ = seqId; };
uint64_t Instruction::getSequenceId() const { return sequenceId_; };

void Instruction::setInstructionId(uint64_t insnId) { instructionId_ = insnId; }
uint64_t Instruction::getInstructionId() const { return instructionId_; }

void Instruction::setFlushed() { flushed_ = true; }
bool Instruction::isFlushed() const { return flushed_; }

bool Instruction::hasExecuted() const { return executed_; }

void Instruction::setCommitReady() { canCommit_ = true; }
bool Instruction::canCommit() const { return canCommit_; }

bool Instruction::hasAllData() const { return (dataPending_ == 0); }

uint16_t Instruction::getLatency() const { return latency_; }
uint16_t Instruction::getLSQLatency() const { return lsqExecutionLatency_; }
uint16_t Instruction::getStallCycles() const { return stallCycles_; }

bool Instruction::isMicroOp() const { return isMicroOp_; }
bool Instruction::isLastMicroOp() const { return isLastMicroOp_; }
void Instruction::setWaitingCommit() { waitingCommit_ = true; }
bool Instruction::isWaitingCommit() const { return waitingCommit_; }
int Instruction::getMicroOpIndex() const { return microOpIndex_; }

uint8_t Instruction::getChildMicroOpCount() const {
  return numberOfChildMicroOps_;
}

void Instruction::registerParentMicroOp(std::shared_ptr<Instruction>& parent) {
  parent->registerChildMicroOp_();
  uopData_ = parent;
}

void Instruction::childMicroOpReady() {
  std::get<0>(uopData_)--;
  assert(std::get<0>(uopData_) >= 0 &&
         "ChildMicroOpCount should not go below 0");
}

void Instruction::notifyParentReady() {
  // if (auto ptr = std::get_if<std::shared_ptr<Instruction>>(&uopData_))
  //   (*ptr)->childMicroOpReady();
  if (uopData_.index() == 1) std::get<1>(uopData_)->childMicroOpReady();
}

bool Instruction::areChildMicroOpsReady() const {
  return std::get<0>(uopData_) == 0;
}
void Instruction::registerChildMicroOp_() {
  std::get<0>(uopData_)++;
  numberOfChildMicroOps_++;
}
}  // namespace simeng
