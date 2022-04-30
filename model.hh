#include <unordered_map>
#include <vector>
#include <queue>
#include <set>

class Machine;
class Invariant;
class SystemState;
class Model;

class Message {
  private:
    Machine& src;
    Machine& dst;

  public:
    Message(Machine& src, Machine& dst);
    virtual void operate(Machine& m);
    virtual ~Message();
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

};

class Invariant {
  public:
    bool check(SystemState state);
};

class SystemState {
  std::vector<Machine> machines;
  std::unordered_map<int, std::vector<Message> > message_queue;

  public:

    SystemState ();

    ~SystemState ();

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
  void run();

};

Message::Message(Machine& src, Machine& dst) : src(src), dst(dst)  {  }

bool Message::operator==(const Message& rhs) const {
  return (this->src == rhs.src) && (this->dst == rhs.dst);
}

bool Machine::operator==(const Machine& rhs) const {
   return this->id == rhs.id;
};


std::vector<SystemState> SystemState::get_neighbors() {
  // TODO
}

bool SystemState::operator==(SystemState const& rhs) const {
  return (machines == rhs.machines) && (message_queue == rhs.message_queue);
}

bool SystemState::operator<(SystemState const& right) const {
  // Todo: actual code here
  return false;
}

Model::Model(SystemState initial_state, std::vector<Invariant> invariants) {
  this->pending.push(initial_state);
  this->invariants = invariants;
}

bool Model::check_invariants(SystemState s) {
  for(auto i : this->invariants) {
    if(!i.check(s)) {
      return false;
    }

  }
  return true;

}

void Model::run() {
  while(!this->pending.empty()) {
    auto current = pending.front();
    pending.pop();
    visited.insert(current);

    check_invariants(current);

    for (auto neighbor : current.get_neighbors())
    {
        if (visited.find(neighbor) != visited.end())
        {
            pending.push(neighbor);
        }
    }
  }
}

