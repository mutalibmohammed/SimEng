#include <memory>

#include "../MockBranchPredictor.hh"
#include "../MockInstruction.hh"
#include "inorder/DecodeUnit.hh"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>

namespace simeng {

using ::testing::_;
using ::testing::Property;
using ::testing::Return;

class InOrderDecodeUnitTest : public testing::Test {
 public:
  InOrderDecodeUnitTest()
      : input(1, {}),
        output(1, nullptr),
        registerFile({1}),
        decodeUnit(input, output, registerFile, predictor),
        uop(new MockInstruction),
        uopPtr(uop),
        sourceRegisters({{0, 0}}) {}

 protected:
  PipelineBuffer<MacroOp> input;
  PipelineBuffer<std::shared_ptr<Instruction>> output;
  RegisterFile registerFile;
  MockBranchPredictor predictor;
  inorder::DecodeUnit decodeUnit;

  MockInstruction* uop;
  std::shared_ptr<Instruction> uopPtr;

  std::vector<Register> sourceRegisters;
};

// Tests that the decode unit output remains empty when an empty macro-op is
// present
TEST_F(InOrderDecodeUnitTest, TickEmpty) {
  decodeUnit.tick();

  EXPECT_EQ(output.getTailSlots()[0], nullptr);
}

// Tests that the decode unit extracts an processes a uop correctly
TEST_F(InOrderDecodeUnitTest, Tick) {
  input.getHeadSlots()[0] = {uopPtr};

  EXPECT_CALL(*uop, checkEarlyBranchMisprediction())
      .WillOnce(Return(std::tuple<bool, uint64_t>(false, 0)));

  decodeUnit.tick();

  // Check result uop is the same as the one provided
  auto result = output.getTailSlots()[0];
  EXPECT_EQ(result.get(), uop);

  // Check no flush was requested
  EXPECT_EQ(decodeUnit.shouldFlush(), false);
}

// Tests that the decode unit requests a flush when a non-branch is mispredicted
TEST_F(InOrderDecodeUnitTest, Flush) {
  input.getHeadSlots()[0] = {uopPtr};

  EXPECT_CALL(*uop, checkEarlyBranchMisprediction())
      .WillOnce(Return(std::tuple<bool, uint64_t>(true, 1)));
  EXPECT_CALL(*uop, isBranch()).WillOnce(Return(false));
  EXPECT_CALL(*uop, getInstructionAddress()).WillOnce(Return(2));

  // Check the predictor is updated with the correct instruction address and PC
  EXPECT_CALL(predictor, update(2, false, 1));

  decodeUnit.tick();

  // Check that a flush was correctly requested
  EXPECT_EQ(decodeUnit.shouldFlush(), true);
  EXPECT_EQ(decodeUnit.getFlushAddress(), 1);
}

// Tests that the decode unit does not forward operands when the uop is ready
TEST_F(InOrderDecodeUnitTest, ForwardReady) {
  output.getTailSlots()[0] = uopPtr;

  EXPECT_CALL(*uop, canExecute()).WillOnce(Return(true));
  EXPECT_CALL(*uop, supplyOperand(_, _)).Times(0);

  std::vector<Register> registers(1);
  std::vector<RegisterValue> values(1);
  decodeUnit.forwardOperands({registers.data(), registers.size()},
                             {values.data(), values.size()});
}

// Tests that the decode unit forwards operands to non-ready instructions
TEST_F(InOrderDecodeUnitTest, ForwardNonReady) {
  output.getTailSlots()[0] = uopPtr;

  std::vector<Register> registers = {{0, 1}};
  std::vector<RegisterValue> values = {RegisterValue(1, 8)};

  // Check that the instruction readiness is verified before supplying operands
  EXPECT_CALL(*uop, canExecute()).WillOnce(Return(false));
  // Check that the forwarded operand is supplied
  EXPECT_CALL(*uop, supplyOperand(registers[0],
                                  Property(&RegisterValue::get<uint64_t>, 1)))
      .Times(1);
  // Check that the source registers are requested
  EXPECT_CALL(*uop, getOperandRegisters())
      .WillOnce(Return(span(sourceRegisters.data(), sourceRegisters.size())));
  // Check that the readiness is checked for provided registers
  EXPECT_CALL(*uop, isOperandReady(0)).WillOnce(Return(true));

  decodeUnit.forwardOperands({registers.data(), registers.size()},
                             {values.data(), values.size()});
}

// Tests that the decode unit supplies remaining operands once forwarding is
// complete
TEST_F(InOrderDecodeUnitTest, ForwardRead) {
  output.getTailSlots()[0] = uopPtr;

  registerFile.set(sourceRegisters[0], RegisterValue(1, 8));

  std::vector<Register> registers;
  std::vector<RegisterValue> values;

  EXPECT_CALL(*uop, canExecute()).WillOnce(Return(false));
  // Check that the source registers are requested
  EXPECT_CALL(*uop, getOperandRegisters())
      .WillOnce(Return(span(sourceRegisters.data(), sourceRegisters.size())));
  // Check that the readiness is checked for provided registers
  EXPECT_CALL(*uop, isOperandReady(0)).WillOnce(Return(false));
  // Check that the correct register and value are supplied
  EXPECT_CALL(*uop, supplyOperand(sourceRegisters[0],
                                  Property(&RegisterValue::get<uint64_t>, 1)))
      .Times(1);

  decodeUnit.forwardOperands({registers.data(), registers.size()},
                             {values.data(), values.size()});
}

}  // namespace simeng
