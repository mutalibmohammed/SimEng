#pragma once

#include "simeng/BranchPredictor.hh"

namespace simeng {

/** An "Always Not Taken" branch predictor; predicts all branches as not taken.
 */
class AlwaysNotTakenPredictor : public BranchPredictor {
 public:
  /** Generate a branch prediction for the specified instruction address; will
   * always predict not taken. */
  BranchPrediction predict(uint64_t address, BranchType type,
                           uint64_t knownTarget) override;

  /** Provide branch results to update the prediction model for the specified
   * instruction address. As this model is static, this does nothing. */
  void update(uint64_t address, bool taken, uint64_t targetAddress,
              BranchType type) override;

  /** Provide flush logic for branch prediction scheme. As there's no flush
   * logic for an always taken predictor, this does nothing. */
  void flush(uint64_t address) override;
};

}  // namespace simeng
