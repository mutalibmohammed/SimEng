#include "SimEngElement.hh"

#include "Memory.hh"

enum class SimulationMode { Emulation, InOrderPipelined, OutOfOrder };

float clockFreq_;
uint32_t timerFreq_;

SimEngElement::SimEngElement(SST::ComponentId_t id, SST::Params& params)
    : SST::Component(id) {
  // Set the prefix to output messages
  output.init(getName() + "-> ", 1, 0, SST::Output::STDOUT);

  auto cpuClock = params.find<std::string>("cpu_clock");
  configPath = params.find<std::string>("config_path");
  cacheLineSize = params.find<uint64_t>("cache_line_size");
  executablePath = params.find<std::string>("executable_path");
  executableArgs = params.find<std::string>("executable_args");

  // Register a plain clock
  timeConverter = registerClock(
      cpuClock,
      new SST::Clock::Handler<SimEngElement>(this, &SimEngElement::clockTick));
  output.verbose(CALL_INFO, 1, 0, "CPU clock configured for %s\n",
                 cpuClock.c_str());

  // Define memory interface
  std::string memInterface = "memHierarchy.memInterface";
  output.verbose(CALL_INFO, 1, 0, "Loading memory interface: %s ...\n",
                 memInterface.c_str());
  SST::Params interfaceParams = params.get_scoped_params("meminterface");
  interfaceParams.insert("port", "cache_link");

  mem = loadAnonymousSubComponent<SimpleMem>(
      memInterface, "memory", 0,
      SST::ComponentInfo::SHARE_PORTS | SST::ComponentInfo::INSERT_STATS,
      interfaceParams, timeConverter,
      new SimpleMem::Handler<SimEngElement>(this, &SimEngElement::handleEvent));

  if (mem == NULL) {
    output.fatal(CALL_INFO, -1, "Error: unable to load %s memory interface.\n",
                 memInterface.c_str());
  } else {
    output.verbose(CALL_INFO, 1, 0,
                   "Memory interface %s successfully loaded.\n",
                   memInterface.c_str());
  }

  // Tell SST to wait until we authorize it to exit
  registerAsPrimaryComponent();
  primaryComponentDoNotEndSim();
}

SimEngElement::~SimEngElement() {}

void SimEngElement::init(unsigned int phase) {
  mem->init(phase);

  if (phase == 0) {
    output.verbose(CALL_INFO, 1, 0, "Setting up SimEng core component..\n");

    std::cout << "Build metadata:" << std::endl;
    std::cout << "\tVersion: " SIMENG_VERSION << std::endl;
    std::cout << "\tCompile Time - Date: " __TIME__ " - " __DATE__ << std::endl;
    std::cout << "\tBuild type: " SIMENG_BUILD_TYPE << std::endl;
    std::cout << "\tCompile options: " SIMENG_COMPILE_OPTIONS << std::endl;
    std::cout << "\tTest suite: " SIMENG_ENABLE_TESTS << std::endl;
    std::cout << std::endl;

    SimulationMode mode = SimulationMode::InOrderPipelined;
    YAML::Node config;

    if (configPath != "") {
      config = simeng::ModelConfig(configPath).getConfigFile();
    } else {
      config = YAML::Load(DEFAULT_CONFIG);
    }

    if (config["Core"]["Simulation-Mode"].as<std::string>() == "emulation") {
      mode = SimulationMode::Emulation;
    } else if (config["Core"]["Simulation-Mode"].as<std::string>() ==
               "outoforder") {
      mode = SimulationMode::OutOfOrder;
    }

    clockFreq_ = config["Core"]["Clock-Frequency"].as<float>();
    timerFreq_ = config["Core"]["Timer-Frequency"].as<uint32_t>();
    timerModulo = (clockFreq_ * 1e9) / (timerFreq_ * 1e6);

    // Create the process image
    std::vector<std::string> commandLine({executablePath, executableArgs});
    process =
        std::make_unique<simeng::kernel::LinuxProcess>(commandLine, config);
    if (!process->isValid()) {
      std::cerr << "Could not read/parse " << executablePath << std::endl;
      exit(1);
    }

    // Read the process image and copy to memory
    auto processImage = process->getProcessImage();
    size_t processMemorySize = processImage.size();
    processMemory = new char[processMemorySize]();
    std::copy(processImage.begin(), processImage.end(), processMemory);

    uint64_t entryPoint = process->getEntryPoint();

    // Create the OS kernel with the process
    kernel = std::make_unique<simeng::kernel::Linux>();
    kernel->createProcess(*process.get());

    instructionMemory = std::make_unique<simeng::FlatMemoryInterface>(
        processMemory, processMemorySize);

    // Create the architecture, with knowledge of the kernel
    arch =
        std::make_unique<simeng::arch::aarch64::Architecture>(*kernel, config);

    predictor = std::make_unique<simeng::BTBPredictor>(
        config["Branch-Predictor"]["BTB-bitlength"].as<uint8_t>());
    auto config_ports = config["Ports"];
    std::vector<std::vector<uint16_t>> portArrangement(config_ports.size());
    // Extract number of ports
    for (size_t i = 0; i < config_ports.size(); i++) {
      auto config_groups = config_ports[i]["Instruction-Group-Support"];
      // Extract number of groups in port
      for (size_t j = 0; j < config_groups.size(); j++) {
        portArrangement[i].push_back(config_groups[j].as<uint16_t>());
      }
    }
    portAllocator = std::make_unique<simeng::pipeline::BalancedPortAllocator>(
        portArrangement);

    // Configure reservation station arrangment
    std::vector<std::pair<uint8_t, uint64_t>> rsArrangement;
    for (size_t i = 0; i < config["Reservation-Stations"].size(); i++) {
      auto reservation_station = config["Reservation-Stations"][i];
      for (size_t j = 0; j < reservation_station["Ports"].size(); j++) {
        uint8_t port = reservation_station["Ports"][j].as<uint8_t>();
        if (rsArrangement.size() < port + 1) {
          rsArrangement.resize(port + 1);
        }
        rsArrangement[port] = {i, reservation_station["Size"].as<uint16_t>()};
      }
    }

    iterations = 0;
    size = processMemorySize;

    std::string modeString = "Out-of-Order";

    dataMemory = std::make_unique<simeng::SSTMemoryInterface>(
        processMemory, processMemorySize, mem, cacheLineSize);

    core = std::make_unique<simeng::models::outoforder::Core>(
        *instructionMemory, *dataMemory, processMemorySize, entryPoint, *arch,
        *predictor, *portAllocator, rsArrangement, config);

    simeng::SpecialFileDirGen SFdir = simeng::SpecialFileDirGen(config);
    // Create the Special Files directory if indicated to do so in Config
    if (config["CPU-Info"]["Generate-Special-Dir"].as<std::string>() == "T") {
      // Remove any current special files dir
      SFdir.RemoveExistingSFDir();
      // Create new special files dir
      SFdir.GenerateSFDir();
    }

    std::vector<uint8_t> image;
    image.reserve(processMemorySize);

    for (size_t i = 0; i < processMemorySize; i++) {
      image.push_back((uint8_t)processMemory[i]);
    }

    SimpleMem::Request* req = new SimpleMem::Request(SimpleMem::Request::Write,
                                                     0, image.size(), image);
    output.verbose(CALL_INFO, 1, 0, "Transferring data to memory...\n");
    mem->sendInitData(req);
    output.verbose(CALL_INFO, 1, 0, "Memory transferred.\n");

    // SimEng core setup complete
    output.verbose(CALL_INFO, 1, 0, "SimEng core setup successfully.\n");

    std::cout << "Running in " << modeString << " mode\n";
    std::cout << "Starting..." << std::endl;
    startTime = std::chrono::high_resolution_clock::now();
  }
}

void SimEngElement::finish() {
  output.verbose(CALL_INFO, 1, 0, "Component is being finished.\n");

  // Finish SimEng core...

  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime)
          .count();
  auto hz = iterations / (static_cast<double>(duration) / 1000.0);
  auto khz = hz / 1000.0;
  auto retired = core->getInstructionsRetiredCount();
  auto mips = retired / static_cast<double>(duration) / 1000.0;

  // Print stats
  std::cout << "\n";
  auto stats = core->getStats();
  for (const auto& [key, value] : stats) {
    std::cout << key << ": " << value << "\n";
  }

  std::cout << "\nFinished " << iterations << " ticks in " << duration << "ms ("
            << std::round(khz) << " kHz, " << std::setprecision(2) << mips
            << " MIPS)" << std::endl;

  delete[] processMemory;
}

void SimEngElement::handleEvent(SimpleMem::Request* event) {
  dataMemory->handleResponse(
      event->cmd == SimpleMem::Request::Command::ReadResp, event->id,
      event->data);
  delete event;
}

bool SimEngElement::clockTick(SST::Cycle_t currentCycle) {
  // Tick the core and memory interfaces until the program has halted
  if (!core->hasHalted() || dataMemory->hasPendingRequests()) {
    // Tick the core
    core->tick();
    // Update Virtual Counter Timer at correct frequency.
    if (iterations % (uint64_t)timerModulo == 0) {
      vitrualCounter++;
      core->incVCT(vitrualCounter);
    }

    // Tick memory
    instructionMemory->tick();
    dataMemory->tick();

    iterations++;

    return false;
  } else {
    primaryComponentOKToEndSim();
    return true;
  }
}
