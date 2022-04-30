#include "model.hh"


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
      TestMessage *t = new TestMessage(this->id, 0);

      std::vector<Message*> results;
      results.push_back(t);
      std::cout << "Sender Machine " << this->id << " sent its message." << std::endl;
      return results;
    }


int main(void) {

  // Our machines are two senders and a receiver.

  std::vector<Machine*> ms;
  ms.push_back(new TestReceiverMachine(0));
  for(int i = 1; i <= 10; i++) {
      ms.push_back(new TestSenderMachine(i));
  }


  // Package up into a model with no invariants.
  SystemState initial_state(ms);
  Model model(initial_state, std::vector<Invariant>());


  auto results = model.run();


  std::cout << "Simulation exited with " << results.size() << " terminating states" << std::endl;
  // Log some stuff about each terminating state.
  // for (auto i : results) {
  //     std::cout << "Final state:" << std::endl;

  //     auto log = dynamic_cast<TestReceiverMachine*>(i.machines[0])->received_log;
  //     std::cout << "Log entries: " << log[0] << log[1] << std::endl;
  // }
  return 0;
}
