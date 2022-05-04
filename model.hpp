#include <unordered_map>
#include <vector>
#include <queue>
#include <set>
#include <string>
#include <iostream>

class Machine;
class Invariant;
class SystemState;
class Model;


class Message {
  // A Message is the basic unit of communication in the model checker

  // This class is pure virtual and should never be instantiated directly.
  // Make subclasses (perhaps with additional state) for any type of message
  // you want to send.

  public:
    int src;
    int dst;

    Message(int src, int dst) {
      this->src = src;
      this->dst = dst;
    }

    virtual ~Message() {};


    // A message is "delivered" to m by mutating its state.
    // This is analogous to RPC
    virtual void operate(Machine& m) {};

    // Must be defined for any message, so that the system states can be comapred.
    // Note that this should be logical comparison, so that two states that represent
    // the same logical state of the system compare equal.
    int compare(Message* rhs) const {
        if (int r = src - rhs->src) return r;
        return dst - rhs->dst;
    }
};

class Machine {
  public:
    // This is the base class for state machines in the system. Subclasses
    // will likely add mutable state.

    // This class is pure virtual and should never be instantiated directly.
    // Make subclasses (perhaps with additional state) for any type of message
    // you want to send.

    // UID determins position in Machine list
    int id;

    int get_id() { return id; };

    // Machines react to receiving messages. A message might handle a message by:
    //  (1) Allowing the message to mutate its state using msg.operate()
    //  (2) Updating its own state accordingly
    //  (3) Returning a vector of new messages that this machine will emit in response.
    // Note that after intiialization, this is the main handler.
    virtual std::vector<Message*> handle_message(Message& msg) { return std::vector<Message*>(); };

    // A machine must be clonable. Be sure to carefully ensure that machines subclasses are cloned
    // properly. The two clones must compare equal by operator==.
    virtual Machine* clone()  const  { return new Machine(); };   // Uses the copy constructor

    // This is the machines initial behavior. On startup a machine might manipulate its own sstate
    // and then return a vector of messages it emits on initialization.
    virtual std::vector<Message*> on_startup() {
      return std::vector<Message*>();
    }

    // Must be defined for any machine, so that the system states can be comapred.
    // Note that this should be logical comparison, so that two states that represent
    // the same logical state of the system compare equal.
    // virtual bool operator==(const Machine& rhs) const;
    virtual int compare(Machine* rhs) const {
        return id - rhs->id;
    }
};



class SystemState final {
public:

  // Together, message_queue and machines constitute the state of a system.

  // The current queue of messages "in the network"
  std::vector<Message*> message_queue;
  // All of the machines
  std::vector<Machine*> machines;

  //
  SystemState (std::vector<Machine*> machines) {
    this->machines = machines;
  }

  // Copy constructor! When we explore the state graph, we deep copy the SystemState.
  // Note that this copies the vectors of pointers, but does not copy the underlying
  // machines or messages. This should be fine, since messages should be immutable.
  // Machines must be manually copied when acted upon, which is handled by the
  // get_neighbors implementation.
  SystemState (const SystemState& other) {
    this->machines = other.machines;
    this->message_queue = other.message_queue;
  }

  // Returns a vector of neighboring states. For now, a next state is reached by
  // any machine accepting any message to it in the message queue. In the future
  // the richness of the simulation will be improved by increasing the dynamics of
  // what constitutes a next state.
  std::vector<SystemState> get_neighbors();

  // SystemStates are comparable so we can skip visited states.
  int compare(const SystemState& rhs) const;
  bool operator==(const SystemState& rhs) const;
  bool operator<(const SystemState& right) const;

  // TODO: This should probably clean up the machines and message_queue.
  // This will take some work to do properly, so for now we leak memory.
  ~SystemState () {};

};

class Invariant {
  // An invariant object is a wrapper around a predicate over the state of a system.
  // Create subclasses to create non-trivial invariants.

  // For convenience, we might want to name invariants.
  public:
    std::string name;

    Invariant(std::string name) {
      this->name = name;
    }

    // An invariant is a predicate over the state of a system, returning
    // true iff the invariant passes the test, and false otherwise.
    // The trivial invariant always returns true.
    virtual bool check(SystemState state) {return true; };

    virtual ~Invariant() {};
};

class Model {

  // The primary model class. A model is a set of states on which we're doing a
  // BFS, essentially. Additionally, it has a set of invariants---predicates which
  // are evaluated on every node.

  public:

  // Pending, visited are the usual BFS roles.
  std::queue<SystemState> pending;
  std::set<SystemState> visited;

  std::vector<Invariant> invariants;

  Model(SystemState initial_state, std::vector<Invariant> invariants);

  // Ensures that s validates against all members of `invariants`.
  bool check_invariants(SystemState s);

  // The primary model checking routine.
  std::vector<SystemState> run();

};

