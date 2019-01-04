#include "A64Instruction.hh"

#include <iostream>

namespace simeng {

void A64Instruction::execute() {
    executed = true;
    switch(opcode) {
        case A64Opcode::B: {
            branchAddress = instructionAddress + metadata.offset;
            return;
        }
        case A64Opcode::LDR_I: {
            results[0].value = memoryData[0].zeroExtend(memoryAddresses[0].second, 8);
            return;
        }
        case A64Opcode::ORR_I: {
            if (metadata.sf) {
                auto value = operands[0].value.get<uint64_t>();
                auto result = (value | (uint64_t)metadata.imm);
                results[0].value = RegisterValue(result);
            } else {
                auto value = operands[0].value.get<uint32_t>();
                auto result = (value | (uint32_t)metadata.imm);
                results[0].value = RegisterValue(result, 8);
            }
            return;
        }
        case A64Opcode::STR_I: {
            memoryData[0] = operands[0].value;
            return;
        }
        default:
            exception = ExecutionNotYetImplemented;
            return;
    }
}

}
