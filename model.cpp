#include "model.hh"


// SystemState Implementation


std::vector<SystemState> SystemState::get_neighbors() {
  // Generate all the states neighboring this.
  // Semantics note: Even though data can be shared between the new and old states,
  // this MUST be invisible. That is, a new state needs to clone a machine if its going to change it.


  std::vector<SystemState> results;

  for (int i = 0; i < this->message_queue.size(); i++) {
    // Consider each message m
    auto m = this->message_queue[i];

    // The SystemState copy constructor copies the machine and message_queue
    // vectors of pointers, but not their contents.
    SystemState next = SystemState(*this);

    // We are going to act on m's recipient, so we need to clone it in the
    // new SystemState.
    next.message_queue.erase(next.message_queue.begin() + i);
    next.machines[m->dst] = next.machines[m->dst]->clone();

    // With this fresh machine object, we can handle the message, possibly receiving
    // some new messages to emit. Put those in the message queue of the new system state.
    Machine& dst_machine = *next.machines[m->dst];
    auto new_messages = dst_machine.handle_message(*m);
    next.message_queue.insert(next.message_queue.end(), new_messages.begin(), new_messages.end());

    // Done preparing this state.
    results.push_back(next);
  }

  return results;
}

bool SystemState::operator==(SystemState const& rhs) const {
  // A necessary evil; SystemStates need to be comparable.
  return (machines == rhs.machines) && (message_queue == rhs.message_queue);
}

bool SystemState::operator<(SystemState const& rhs) const {
  return (message_queue == rhs.message_queue) ? (machines < rhs.machines)
    : message_queue < rhs.message_queue;
}

// To construct a Model from an initial state and some invariants, run all of the
// Machine's initialization tasks.
Model::Model(SystemState initial_state, std::vector<Invariant> invariants) {

  this->invariants = invariants;

  // Initial action for each machine.
  for(auto &m : initial_state.machines) {
    auto new_messages = m->on_startup();
    initial_state.message_queue.insert(initial_state.message_queue.end(), new_messages.begin(), new_messages.end());
  }

  // Visit this state first.
  this->pending.push(initial_state);


  std::cout << "Initialized a new model with "
            << initial_state.machines.size()
            << " machines and "
            << invariants.size()
            << " invariants. "
            << std::endl;
}

bool Model::check_invariants(SystemState s) {
  // Check every invariant of the model on s.
  // This can be made more expressive later.
  for(auto i : this->invariants) {
    if(!i.check(s)) {
      std::cout << "INVARIANT VIOLATED: "
                << i.name
                << std::endl;
      return false;
    }
  }
  return true;
}

std::vector<SystemState> Model::run() {
  // Run the model to completion using BFS. Returns a list of states in which
  // the model can terminate.
  std::vector<SystemState> terminating;

  while(!this->pending.empty()) {
    // Extract the next state
    auto current = pending.front();
    pending.pop();

    visited.insert(current);

    bool passed_invariants = check_invariants(current);

    if (not passed_invariants) {
      std::cout << "Model error! Did not pass invariants." << std::endl;

    }

    // To add to the frontier
    auto neighbors = current.get_neighbors();

    // A state with no neighbors is a terminating state of the system. We might be
    // interested in these. For now, we just return a list of them.
    if(neighbors.size() == 0) {
      terminating.push_back(current);
    }

    // Add all non-visisted neighbors to the queue. Note that this relies on proper
    // operator== operations for all machines and messages.
    for (auto neighbor : neighbors)
    {
        if (visited.find(neighbor) != visited.end())
        {
            pending.push(neighbor);
        }
    }

  }
  return terminating;
}
