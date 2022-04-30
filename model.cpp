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

bool SystemState::operator<(SystemState const& right) const {
  // TODO: create an ordering so we can have sets. This is urgent.
  return false;
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

// -------------------------------------------------------------
// ---------------------TO BE MOVED-----------------------------
// -------------------------------------------------------------


// A simple example of two sender machines sending messages to a single receiver.
// Due to network asynchrony, the receiver may receive them in any order.

class TestMessage;
class TestSenderMachine;
class TestReceiverMachine;

class TestSenderMachine : public Machine {
public:

    TestSenderMachine(int id) {
      this->id = id;
    }

    // A sender sends a machine on startup immediately. See implementation below.
    std::vector<Message*> on_startup() override;

    bool operator==(const TestSenderMachine& rhs) const  {
      return this->id == rhs.id;
    }

    TestSenderMachine* clone()  const override {
      return new TestSenderMachine(this->id);
    }

};

class TestReceiverMachine : public Machine {
public:
    // A receiver has an ordered log of the ids of machines from whom it has received
    // messages.
    std::vector<int> received_log;

    TestReceiverMachine(int id) {
      this->id = id;
    }

    TestReceiverMachine(int id, std::vector<int> received_log) {
      this->id = id;
      this->received_log = received_log;
    }


    std::vector<Message*> handle_message(Message& msg)  override {
      msg.operate(*this);
      // A receiver lets a message operate on it. These will be TestMessage objects
      // which update the log.
      return std::vector<Message*>();
    };

    // A receiver does nothing on startup.
    std::vector<Message*> on_startup()  override { return std::vector<Message*>(); }

    bool operator==(const TestSenderMachine& rhs) const {
      return this->id == rhs.id;
    }

    TestReceiverMachine* clone()  const override {
      return new TestReceiverMachine(this->id, this->received_log);
    }
};

class TestMessage : public Message {
    using Message::Message;

    // A test message puts its src id in its recipients log. That's it.
    void operate(Machine& m) override {
      dynamic_cast<TestReceiverMachine&>(m).received_log.push_back(this->src);
    }

};


std::vector<Message*> TestSenderMachine::on_startup()
    {
      // Machine id 2 is going to be our receiver
      TestMessage *t = new TestMessage(this->id, 2);

      std::vector<Message*> results;
      results.push_back(t);
      std::cout << "Sender Machine " << this->id << " sent its message." << std::endl;
      return results;
    }


int main(void) {
  // Our machines are two senders and a receiver.
  std::vector<Machine*> ms;
  ms.push_back(new TestSenderMachine(0));
  ms.push_back(new TestSenderMachine(1));
  ms.push_back(new TestReceiverMachine(2));

  // Package up into a model with no invariants.
  SystemState initial_state(ms);
  Model model(initial_state, std::vector<Invariant>());


  auto results = model.run();


  std::cout << "Simulation exited with " << results.size() << " terminating states" << std::endl;
  // Log some stuff about each terminating state.
  for (auto i : results) {
      std::cout << "Final state:" << std::endl;

      auto log = dynamic_cast<TestReceiverMachine*>(i.machines[2])->received_log;
      std::cout << "Log entries: " << log[0] << log[1] << std::endl;
  }
  return 0;
}
