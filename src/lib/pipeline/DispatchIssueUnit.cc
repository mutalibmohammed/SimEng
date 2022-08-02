#include "simeng/pipeline/DispatchIssueUnit.hh"

#include <algorithm>
#include <iostream>

namespace simeng {
namespace pipeline {

DispatchIssueUnit::DispatchIssueUnit(
    PipelineBuffer<std::shared_ptr<Instruction>>& fromRename,
    std::vector<PipelineBuffer<std::shared_ptr<Instruction>>>& issuePorts,
    const RegisterFileSet& registerFileSet, PortAllocator& portAllocator,
    const std::vector<uint16_t>& physicalRegisterStructure, YAML::Node config,
    Statistics& stats)
    : input_(fromRename),
      issuePorts_(issuePorts),
      registerFileSet_(registerFileSet),
      scoreboard_(physicalRegisterStructure.size()),
      dependencyMatrix_(physicalRegisterStructure.size()),
      portAllocator_(portAllocator),
      stats_(stats) {
  // Initialise scoreboard
  for (size_t type = 0; type < physicalRegisterStructure.size(); type++) {
    scoreboard_[type].assign(physicalRegisterStructure[type], true);
    dependencyMatrix_[type].resize(physicalRegisterStructure[type]);
  }
  // Create set of reservation station structs with correct issue port
  // mappings
  for (size_t i = 0; i < config["Reservation-Stations"].size(); i++) {
    // Iterate over each reservation station in config
    auto reservation_station = config["Reservation-Stations"][i];
    // Create ReservationStation struct to be stored
    ReservationStation rs = {
        reservation_station["Size"].as<uint16_t>(),
        reservation_station["Dispatch-Rate"].as<uint16_t>(),
        0,
        {}};
    // Resize rs port attribute to match what's defined in config file
    rs.ports.resize(reservation_station["Ports"].size());
    for (size_t j = 0; j < reservation_station["Ports"].size(); j++) {
      // Iterate over issue ports in config
      uint16_t issue_port = reservation_station["Ports"][j].as<uint16_t>();
      rs.ports[j].issuePort = issue_port;
      // Add port mapping entry, resizing vector if needed
      if ((issue_port + 1) > portMapping_.size()) {
        portMapping_.resize((issue_port + 1));
      }
      portMapping_[issue_port] = {i, j};
    }
    reservationStations_.push_back(rs);
  }
  for (uint16_t i = 0; i < reservationStations_.size(); i++)
    flushed_.emplace(i, std::initializer_list<std::shared_ptr<Instruction>>{});

  // Register stat counters
  rsStallsCntr_ = stats_.registerStat("dispatch.rsStalls");
  frontendStallsCntr_ = stats_.registerStat("issue.frontendStalls");
  backendStallsCntr_ = stats_.registerStat("issue.backendStalls");
  portBusyStallsCntr_ = stats_.registerStat("issue.portBusyStalls");
  for (int p = 0; p < config["Ports"].size(); p++) {
    std::string port_name = config["Ports"][p]["Portname"].as<std::string>();
    portStats_.push_back(
        {stats_.registerStat("issue.possibleIssues." + port_name),
         stats_.registerStat("issue.actualIssues." + port_name),
         stats_.registerStat("issue.frontendSlotStalls." + port_name),
         stats_.registerStat("issue.backendSlotStalls." + port_name),
         stats_.registerStat("issue.portBusySlotStalls." + port_name)});
  }
}

void DispatchIssueUnit::tick() {
  input_.stall(false);

  /** Stores the number of instructions dispatched for each
   * reservation station. */
  std::vector<uint16_t> dispatches = {
      0, static_cast<unsigned short>(reservationStations_.size())};

  for (size_t slot = 0; slot < input_.getWidth(); slot++) {
    auto& uop = input_.getHeadSlots()[slot];
    if (uop == nullptr) {
      continue;
    }

    const std::vector<uint16_t>& supportedPorts = uop->getSupportedPorts();
    if (uop->exceptionEncountered()) {
      // Exception; mark as ready to commit, and remove from pipeline
      uop->setCommitReady();
      input_.getHeadSlots()[slot] = nullptr;
      continue;
    }
    // Allocate issue port to uop
    uint16_t port = portAllocator_.allocate(supportedPorts);
    uint16_t RS_Index = portMapping_[port].first;
    uint16_t RS_Port = portMapping_[port].second;
    assert(RS_Index < reservationStations_.size() &&
           "Allocated port inaccessible");
    ReservationStation& rs = reservationStations_[RS_Index];

    // When appropriate, stall uop or input buffer if stall buffer full
    if (rs.currentSize == rs.capacity ||
        dispatches[RS_Index] == rs.dispatchRate) {
      // Deallocate port given
      portAllocator_.deallocate(port);
      input_.stall(true);
      rsStalls_++;
      stats_.incrementStat(rsStallsCntr_, 1);
      return;
    }

    // Assume the uop will be ready
    bool ready = true;

    // Register read
    // Identify remaining missing registers and supply values
    auto& sourceRegisters = uop->getOperandRegisters();
    for (uint8_t i = 0; i < sourceRegisters.size(); i++) {
      const auto& reg = sourceRegisters[i];

      if (!uop->isOperandReady(i)) {
        // The operand hasn't already been supplied
        if (scoreboard_[reg.type][reg.tag]) {
          // The scoreboard says it's ready; read and supply the register value
          uop->supplyOperand(i, registerFileSet_.get(reg));
        } else {
          // This register isn't ready yet. Register this uop to the dependency
          // matrix for a more efficient lookup later
          dependencyMatrix_[reg.type][reg.tag].push_back({uop, port, i});
          ready = false;
        }
      }
    }

    // Set scoreboard for all destination registers as not ready
    auto& destinationRegisters = uop->getDestinationRegisters();
    for (const auto& reg : destinationRegisters) {
      scoreboard_[reg.type][reg.tag] = false;
    }

    // Increment dispatches made and RS occupied entries size
    dispatches[RS_Index]++;
    rs.currentSize++;
    rs.ports[RS_Port].currentSize++;

    if (ready) {
      rs.ports[RS_Port].ready.push_back(std::move(uop));
    }

    input_.getHeadSlots()[slot] = nullptr;
  }
}

void DispatchIssueUnit::issue() {
  int issued = 0;
  // Check the ready queues, and issue an instruction from each if the
  // corresponding port isn't blocked
  for (size_t i = 0; i < issuePorts_.size(); i++) {
    ReservationStation& rs = reservationStations_[portMapping_[i].first];
    auto& queue = rs.ports[portMapping_[i].second].ready;
    if (issuePorts_[i].isStalled()) {
      if (queue.size() > 0) {
        portBusyStalls_++;
        stats_.incrementStat(portBusyStallsCntr_, 1);
        stats_.incrementStat(portStats_[i].portBusySlotStallsCntr, 1);
        stats_.incrementStat(portStats_[i].backendSlotStallsCntr, 1);
      }
      continue;
    }

    if (queue.size() > 0) {
      auto& uop = queue.front();

      for (const auto& avail : uop->getSupportedPorts()) {
        stats_.incrementStat(portStats_[avail].possibleIssuesCntr, 1);
      }
      stats_.incrementStat(portStats_[i].actualIssuesCntr, 1);

      issuePorts_[i].getTailSlots()[0] = std::move(uop);
      queue.pop_front();

      // Inform the port allocator that an instruction issued
      portAllocator_.issued(i);
      issued++;

      assert(rs.currentSize > 0);
      rs.currentSize--;
      assert(rs.ports[portMapping_[i].second].currentSize > 0);
      rs.ports[portMapping_[i].second].currentSize--;
    } else {
      if (rs.ports[portMapping_[i].second].currentSize != 0)
        stats_.incrementStat(portStats_[i].backendSlotStallsCntr, 1);
      else
        stats_.incrementStat(portStats_[i].frontendSlotStallsCntr, 1);
    }
  }

  if (issued == 0) {
    for (const auto& rs : reservationStations_) {
      if (rs.currentSize != 0) {
        backendStalls_++;
        stats_.incrementStat(backendStallsCntr_, 1);
        return;
      }
    }
    frontendStalls_++;
    stats_.incrementStat(frontendStallsCntr_, 1);
  }
}

void DispatchIssueUnit::forwardOperands(const span<Register>& registers,
                                        const span<RegisterValue>& values) {
  assert(registers.size() == values.size() &&
         "Mismatched register and value vector sizes");

  for (size_t i = 0; i < registers.size(); i++) {
    const auto& reg = registers[i];
    // Flag scoreboard as ready now result is available
    scoreboard_[reg.type][reg.tag] = true;

    // Supply the value to all dependent uops
    const auto& dependents = dependencyMatrix_[reg.type][reg.tag];
    for (auto& entry : dependents) {
      entry.uop->supplyOperand(entry.operandIndex, values[i]);
      if (entry.uop->canExecute()) {
        // Add the now-ready instruction to the relevant ready queue
        auto rsInfo = portMapping_[entry.port];
        reservationStations_[rsInfo.first].ports[rsInfo.second].ready.push_back(
            std::move(entry.uop));
      }
    }

    // Clear the dependency list
    dependencyMatrix_[reg.type][reg.tag].clear();
  }
}

void DispatchIssueUnit::setRegisterReady(Register reg) {
  scoreboard_[reg.type][reg.tag] = true;
}

void DispatchIssueUnit::purgeFlushed() {
  for (size_t i = 0; i < reservationStations_.size(); i++) {
    // Search the ready queues for flushed instructions and remove them
    auto& rs = reservationStations_[i];
    for (auto& port : rs.ports) {
      // Ready queue
      auto readyIter = port.ready.begin();
      while (readyIter != port.ready.end()) {
        auto& uop = *readyIter;
        if (uop->isFlushed()) {
          portAllocator_.deallocate(port.issuePort);
          readyIter = port.ready.erase(readyIter);
          assert(rs.currentSize > 0);
          rs.currentSize--;
          assert(port.currentSize > 0);
          port.currentSize--;
        } else {
          readyIter++;
        }
      }
    }
  }

  // Collect flushed instructions and remove them from the dependency matrix
  for (auto& it : flushed_) it.second.clear();
  for (auto& registerType : dependencyMatrix_) {
    for (auto& dependencyList : registerType) {
      auto it = dependencyList.begin();
      while (it != dependencyList.end()) {
        auto& entry = *it;
        if (entry.uop->isFlushed()) {
          auto rsIndex = portMapping_[entry.port].first;
          if (!flushed_[rsIndex].count(entry.uop)) {
            flushed_[rsIndex].insert(entry.uop);
            portAllocator_.deallocate(entry.port);
            auto rsInfo = portMapping_[entry.port];
            assert(reservationStations_[rsInfo.first]
                       .ports[rsInfo.second]
                       .currentSize > 0);
            reservationStations_[rsInfo.first]
                .ports[rsInfo.second]
                .currentSize--;
          }
          it = dependencyList.erase(it);
        } else {
          it++;
        }
      }
    }
  }

  // Update reservation station size
  for (uint8_t i = 0; i < reservationStations_.size(); i++) {
    assert(reservationStations_[i].currentSize >= flushed_[i].size());
    reservationStations_[i].currentSize -= flushed_[i].size();
  }
}

uint64_t DispatchIssueUnit::getRSStalls() const { return rsStalls_; }
uint64_t DispatchIssueUnit::getFrontendStalls() const {
  return frontendStalls_;
}
uint64_t DispatchIssueUnit::getBackendStalls() const { return backendStalls_; }
uint64_t DispatchIssueUnit::getPortBusyStalls() const {
  return portBusyStalls_;
}

void DispatchIssueUnit::getRSSizes(std::vector<uint64_t>& sizes) const {
  for (auto& rs : reservationStations_) {
    sizes.push_back(rs.capacity - rs.currentSize);
  }
}

}  // namespace pipeline
}  // namespace simeng
