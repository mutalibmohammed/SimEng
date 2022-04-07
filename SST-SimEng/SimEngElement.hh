#ifndef _SIMENG_ELEMENT_H
#define _SIMENG_ELEMENT_H

#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include <sst/core/component.h>
#include <sst/core/eli/elementinfo.h>
#include <sst/core/interfaces/simpleMem.h>
#include <sst/core/sst_config.h>

#include "simeng/AlwaysNotTakenPredictor.hh"
#include "simeng/BTBPredictor.hh"
#include "simeng/BTB_BWTPredictor.hh"
#include "simeng/Core.hh"
#include "simeng/Elf.hh"
#include "simeng/FixedLatencyMemoryInterface.hh"
#include "simeng/FlatMemoryInterface.hh"
#include "simeng/ModelConfig.hh"
#include "simeng/SpecialFileDirGen.hh"
#include "simeng/arch/Architecture.hh"
#include "simeng/arch/aarch64/Architecture.hh"
#include "simeng/arch/aarch64/Instruction.hh"
#include "simeng/arch/aarch64/MicroDecoder.hh"
#include "simeng/kernel/Linux.hh"
#include "simeng/models/emulation/Core.hh"
#include "simeng/models/inorder/Core.hh"
#include "simeng/models/outoforder/Core.hh"
#include "simeng/pipeline/A64FXPortAllocator.hh"
#include "simeng/pipeline/BalancedPortAllocator.hh"
#include "simeng/version.hh"

#include "Memory.hh"

using namespace SST::Interfaces;

class SimEngElement : public SST::Component {
public:
  SimEngElement(SST::ComponentId_t id, SST::Params &params);
  ~SimEngElement();

  void init(unsigned int phase);
  void finish();

  bool clockTick(SST::Cycle_t currentCycle);

  SST_ELI_REGISTER_COMPONENT(SimEngElement, "SimEngElement", "SimEngElement",
                             SST_ELI_ELEMENT_VERSION(1, 0, 0),
                             "An SST wrapped SimEng core that communicates "
                             "with a simpleMem interface.",
                             COMPONENT_CATEGORY_PROCESSOR)

  SST_ELI_DOCUMENT_PARAMS(
      {"cpu_clock", "The frequency of the core"},
      {"cache_line_size", "The size of a cache line"},
      {"executable_path", "The path of the program that SimEng should execute"})

  SST_ELI_DOCUMENT_PORTS({"cache_link", "Connects the core to a cache", {}})

private:
  void handleEvent(SimpleMem::Request *event);
  SST::Output output;
  SST::TimeConverter *timeConverter;
  SimpleMem *mem;

  std::string executablePath;
  std::string executableArgs;
  std::string configPath;
  uint64_t cacheLineSize;
  std::unique_ptr<simeng::kernel::LinuxProcess> process;
  std::unique_ptr<simeng::kernel::Linux> kernel;
  char *processMemory;
  std::unique_ptr<simeng::arch::Architecture> arch;
  std::unique_ptr<simeng::MemoryInterface> instructionMemory;
  std::unique_ptr<simeng::BranchPredictor> predictor;
  std::unique_ptr<simeng::pipeline::PortAllocator> portAllocator;

  int iterations;
  int vitrualCounter;
  double timerModulo;
  int size;

  std::unique_ptr<simeng::SSTMemoryInterface> dataMemory;
  std::unique_ptr<simeng::Core> core;
  std::chrono::high_resolution_clock::time_point startTime;
};

#endif