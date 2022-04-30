#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <string>

class Machine;
class Invariant;
class SystemState;
class Model;

class Message {
  private:
    int src;
    int dst;

  public:
    Message(int src, int dst);
    virtual void operate(Machine& m) {};
    virtual ~Message() {};
    virtual bool operator==(const Message& rhs) const;
};

class Machine {
  private:

    int id;
    // any real machine should have some actual state

  public:
    int get_id() { return id; };

    virtual std::vector<Message> handle_message(Message msg) { return std::vector<Message>(); };
    virtual std::vector<Message> on_startup() { return std::vector<Message>(); }

    virtual bool operator==(const Machine& rhs) const;
    bool operator<(Machine const& right) {return this->id < right.id; };

};

class Invariant {
  std::string name;
  public:
    Invariant(std::string name) {
      this->name = name;
    }
    bool check(SystemState state);
};

class SystemState {
  public:

  std::vector<Message> message_queue;

  std::vector<Machine> machines;

    SystemState (std::vector<Machine> machines) {
      this->machines = machines;
    }

    ~SystemState () {};

    std::vector<SystemState> get_neighbors();

    bool operator==(SystemState const& rhs) const;

    bool operator<(SystemState const& right) const;

};

class Model {

  std::queue<SystemState> pending;
  std::set<SystemState> visited;
  std::vector<Invariant> invariants;

  Model(SystemState initial_state, std::vector<Invariant> invariants);
  bool check_invariants(SystemState s);
  std::vector<SystemState> run();

};



