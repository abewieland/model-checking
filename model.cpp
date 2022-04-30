#include "model.hh"


Message::Message(int src, int dst) {
  this->src = src;
  this->dst = dst;
}

// virtual void operate(Machine& m) {}

bool Message::operator==(const Message& rhs) const {
  return (this->src == rhs.src) && (this->dst == rhs.dst);
}

bool Machine::operator==(const Machine& rhs) const {
   return this->id == rhs.id;
};


std::vector<SystemState> SystemState::get_neighbors() {
  std::vector<SystemState> results;

  for (auto m : this->message_queue) {
    // either take m or don't

    Machine dst_machine = this->machines[m.dst];

  }



  // TODO
}

bool SystemState::operator==(SystemState const& rhs) const {
  return (machines == rhs.machines) && (message_queue == rhs.message_queue);
}

bool SystemState::operator<(SystemState const& right) const {
  // TODO: ordering on these guys or write a hash
  // return this->machines < right.machines;
  return false;
}

Model::Model(SystemState initial_state, std::vector<Invariant> invariants) {
  this->pending.push(initial_state);
  this->invariants = invariants;
}

bool Invariant::check(SystemState state) {
  return false;
}

bool Model::check_invariants(SystemState s) {
  for(auto i : this->invariants) {
    if(!i.check(s)) {
      return false;
    }

  }
  return true;

}

std::vector<SystemState> Model::run() {

  std::vector<SystemState> terminating;

  while(!this->pending.empty()) {
    auto current = pending.front();
    pending.pop();
    visited.insert(current);

    bool passed_invariants = check_invariants(current);

    auto neighbors = current.get_neighbors();
    if(neighbors.size() == 0) {
      terminating.push_back(current);
    }

    for (auto neighbor : neighbors)
    {
        if (visited.find(neighbor) != visited.end())
        {
            pending.push(neighbor);
        }
    }

    return terminating;
  }
}



int main(void) {

}
