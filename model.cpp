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

  for (int i = 0; i < this->message_queue.size(); i++) {
    auto m = this->message_queue[i];
    SystemState next = SystemState(*this);

    std::cout << "found " << results.size() << "neighbors" << std::endl;

    next.message_queue.erase(next.message_queue.begin() + i);
    Machine dst_machine = *next.machines[m.dst];
    auto new_messages = dst_machine.handle_message(m);
    next.message_queue.insert(next.message_queue.end(), new_messages.begin(), new_messages.end());
    results.push_back(next);
  }
    std::cout << "found " << results.size() << "neighbors" << std::endl;

  return results;
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

  this->invariants = invariants;

  for(auto &m : initial_state.machines) {
    auto new_messages = m->on_startup();
    std::cout << "Inited a machine and found" << new_messages.size() << "machines" << std::endl;

    initial_state.message_queue.insert(initial_state.message_queue.end(), new_messages.begin(), new_messages.end());
  }

  this->pending.push(initial_state);
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

  }
    return terminating;

}

// A simple example

class TestMessage;
class TestSenderMachine;
class TestReceiverMachine;


class TestSenderMachine : public Machine {
public:

    TestSenderMachine(int id) {
      this->id = id;
    }

    // Ignores all messagnes
    std::vector<Message> handle_message(Message msg)  override {    } ;

    // Sends right away to machine 2
    std::vector<Message> on_startup() override;


    bool operator==(const TestSenderMachine& rhs) const  {
      return this->id == rhs.id;
    }

};

class TestReceiverMachine : public Machine {
public:
    TestReceiverMachine(int id) {
      this->id = id;
    }

    std::vector<int> received_log;

    std::vector<Message> handle_message(Message msg)  override {
      msg.operate(*this);
    };


    // Does nothing on startup
    std::vector<Message> on_startup()  override { }

    bool operator==(const TestSenderMachine& rhs) const {
      return this->id == rhs.id;
    }
};

class TestMessage : public Message {
    using Message::Message;

    void operate(Machine& m) {
      dynamic_cast<TestReceiverMachine&>(m).received_log.push_back(this->src);

    }

};

std::vector<Message> TestSenderMachine::on_startup()
    {
      TestMessage t(this->id, 2);
      std::vector<Message> results;
      results.push_back(t);
      std::cout << "send startup" << std::endl;

      return results;
    }

int main(void) {
  std::vector<Machine*> ms;
  ms.push_back(new TestSenderMachine(0));
  ms.push_back(new TestSenderMachine(1));
  ms.push_back(new TestReceiverMachine(2));

  SystemState initial_state(ms);

  Model model(initial_state, std::vector<Invariant>());
  auto results = model.run();
  std::cout << "Got " << results.size() << " results";

  std::cout << "finished" << std::endl;
  std::cout << "Inited a machine." << std::endl;


}
