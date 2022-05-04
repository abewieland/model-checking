#include <vector>
#include <queue>
#include <set>
#include <string>
#include <functional>
#include <iostream>

class Machine;

// Machine identifiers
typedef unsigned id_t;

struct Message {
    // A Message is the basic unit of communication in the model checker; they
    // are currently immutable and always maintained in memory.

    // This class is purely virtual and should never be instantiated directly.
    // Make subclasses (perhaps with additional state) for more specific
    // messages.

    id_t src;
    id_t dst;

    Message(id_t src, id_t dst) : src(src), dst(dst) {}
    virtual ~Message() {}

    // A message is "delivered" to m by mutating its state.
    // This is analogous to RPC
    // XXX is this still a useful abstraction?
    virtual void operate(Machine& m) {}

    // Compare this message to `rhs` in a classic three-way manner; should be
    // overridden by subclasses if they add more members.
    virtual int compare(Message* rhs) const {
        // First compare type, then source and destination
        if (typeid(*this) != typeid(*rhs))
            return typeid(*this).before(typeid(*rhs)) ? -1 : 1;
        if (int r = src - rhs->src) return r;
        return dst - rhs->dst;
    }
};

struct Machine {
    // This is the base class for state machines in the system. Subclasses
    // will likely add mutable state.

    // This class is purely virtual and should never be instantiated directly.
    // Make subclasses (with additional state) for more specific machines.

    // id currently determins position in machine list, so must be non-negative.
    id_t id;

    Machine(id_t id) : id(id) {}
    virtual ~Machine() {}

    // Machines react to receiving messages by:
    //  (1) Updating their state accordingly
    //  (2) Returning a vector of new messages to emit in response
    // Note that after intiialization, this is the main handler.
    virtual std::vector<Message*> handle_message(Message* msg) {
        return std::vector<Message*>{};
    }

    // A machine must be clonable to allow for mutation. Subclasses must
    // implement this method such that `compare(clone()) == 0`.
    virtual Machine* clone() const {
        return new Machine(id);
    }

    // On startup a machine might manipulate its own state, then return a vector
    // of messages it emits on initialization.
    virtual std::vector<Message*> on_startup() {
        return std::vector<Message*>{};
    }

    // Compare this message to `rhs` in a classic three-way manner; should be
    // overridden by subclasses if they add more members.
    virtual int compare(Machine* rhs) const {
        return (int) id - rhs->id;
    }
};

struct SystemState final {
    // Together, the messages and machines constitute the state of a system.
    std::vector<Message*> messages;
    std::vector<Machine*> machines;

    // Initialize with a machine list.
    SystemState (std::vector<Machine*> machines) : machines(machines) {}

    // When we explore the state graph, we deep copy the SystemState. This
    // copies the vectors of pointers, but does not copy the underlying machines
    // or messages. This should be fine, since messages are immutable and
    // machines must be manually copied when acted upon, which is handled by the
    // get_neighbors implementation.
    SystemState (const SystemState& other) {
        this->machines = other.machines;
        this->messages = other.messages;
    }

    // SystemStates are comparable so we can skip visited states.
    int compare(const SystemState& rhs) const;
    bool operator==(const SystemState& rhs) const {
        return !compare(rhs);
    }
    bool operator<(const SystemState& rhs) const {
        return compare(rhs) < 0;
    }

    // TODO: This should probably clean up the machines and message_queue.
    // This will take some work to do properly, so for now we leak memory.
    ~SystemState () {}
};

struct Invariant final {
    // An invariant is simply a named predicate over the state of a system.
    std::string name;
    std::function<bool(SystemState)> check;

    Invariant(std::string s, std::function<bool(SystemState)> fn)
        : name(s), check(fn) {}
};

struct Diff final {
    // A diff simply captures the messages sent and delivered; only one message
    // is ever delivered for each diff, so no need for a vector there
    std::vector<Message*> sent;
    Message* delivered;
};

struct History final {
    // A history is just a state and an ordered list of diffs that led from the
    // initial state to this state
    SystemState curr;
    std::vector<Diff> history;

    // Make a history from a state (set the history later)
    History(SystemState s) : curr(s) {}

    // Returns a vector of neighboring states, with the added methods to get
    // there. For now, a next state is reached by any machine accepting a
    // message from the queue.
    std::vector<History> get_neighbors();
};

struct Model final {
    // A model is a set of states on which we're doing a BFS, essentially.
    // It also has a set of invariants evaluated at each state, and a history
    // to arrive at each state.

    // Pending, visited are the usual BFS roles.
    std::queue<History> pending;
    std::set<SystemState> visited;

    std::vector<Invariant> invariants;

    // Initialize a model with an initial state (a vector of machines) and some
    // invariants
    Model(std::vector<Machine*> m, std::vector<Invariant> i);

    // Ensures that `s` validates against all members of `invariants`.
    bool check_invariants(SystemState s) const {
        for (const Invariant& i : invariants) {
            if (!i.check(s)) {
                std::cerr << "INVARIANT VIOLATED: " << i.name << std::endl;
                return false;
            }
        }
        return true;
    }

    // The primary model checking routine; returns a list of states the model
    // may terminate in.
    std::vector<SystemState> run();
};
