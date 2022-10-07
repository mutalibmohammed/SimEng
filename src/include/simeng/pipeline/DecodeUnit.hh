#pragma once

#include <queue>

#include "simeng/Statistics.hh"
#include "simeng/arch/Architecture.hh"
#include "simeng/pipeline/PipelineBuffer.hh"

namespace simeng {
namespace pipeline {

/** A decode unit for a pipelined processor. Splits pre-decoded macro-ops into
 * uops. */
class DecodeUnit {
 public:
  /** Constructs a decode unit with references to input/output buffers and the
   * current branch predictor. */
  DecodeUnit(PipelineBuffer<MacroOp>& input,
             PipelineBuffer<std::shared_ptr<Instruction>>& output,
             BranchPredictor& predictor, Statistics& stats);

  /** Ticks the decode unit. Breaks macro-ops into uops, and performs early
   * branch misprediction checks. */
  void tick();

  /** Check whether the core should be flushed this cycle. */
  bool shouldFlush() const;

  /** Retrieve the target instruction address associated with the most recently
   * discovered misprediction. */
  uint64_t getFlushAddress() const;

  /** Clear the microOps_ queue. */
  void purgeFlushed();

 private:
  /** A buffer of macro-ops to split into uops. */
  PipelineBuffer<MacroOp>& input_;
  /** An internal buffer for storing one or more uops. */
  std::deque<std::shared_ptr<Instruction>> microOps_;
  /** A buffer for writing decoded uops into. */
  PipelineBuffer<std::shared_ptr<Instruction>>& output_;

  /** A reference to the current branch predictor. */
  BranchPredictor& predictor_;

  /** Whether the core should be flushed after this cycle. */
  bool shouldFlush_;

  /** The target instruction address the PC should be updated to upon flush. */
  uint64_t pc_;

  /** A reference to the Statistics class. */
  Statistics& stats_;

  /** Statistics class id for the number of times that the decode unit requested
   * a flush due to discovering a branch misprediction early. */
  uint64_t earlyFlushesCntr_;
};

}  // namespace pipeline
}  // namespace simeng
