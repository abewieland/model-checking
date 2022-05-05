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
    // are currently immutable and always maintained in memory. Some simple
    // messages are expressible just via different types; others may need to be
    // made subclasses to add members (if so, they *must* override compare to
    // effectively differentiate based on new state)

    id_t src;
    id_t dst;
    int type;

    Message(id_t src, id_t dst, int type) : src(src), dst(dst), type(type) {}
    virtual ~Message() {}

    // Compare this message to `rhs` in a classic three-way manner
    int compare(Message* rhs) const {
        // First compare type, then source and destination
        if (int r = type - rhs->type) return r;
        if (int r = (int) src - rhs->src) return r;
        if (int r = (int) dst - rhs->dst) return r;
        return sub_compare(rhs);
    }

    // Compare this message to `rhs`; must be overridden by subclasses if they
    // add members
    virtual int sub_compare(Message* rhs) const {
        return 0;
    }
};

struct Machine {
    // This is the base class for state machines in the system. Subclasses
    // will likely add mutable state.

    // This class is purely virtual and may never be instantiated directly.
    // Make subclasses (with additional state) for specific machines.

    // id currently determines position in machine list, so must be non-negative.
    id_t id;

    Machine(id_t id) : id(id) {}
    virtual ~Machine() {}

    // Machines react to receiving messages by:
    //  (1) Updating their state accordingly
    //  (2) Returning a vector of new messages to emit in response
    // Note that after initialization, this is the main handler.
    virtual std::vector<Message*> handle_message(Message* msg) {
        return std::vector<Message*>{};
    }

    // A machine must be cloneable to allow for mutation. Subclasses must
    // implement this method such that `compare(clone()) == 0`.
    virtual Machine* clone() const = 0;

    // On startup a machine might manipulate its own state, then return a vector
    // of messages it emits on initialization.
    virtual std::vector<Message*> on_startup() {
        return std::vector<Message*>{};
    }

    // Compare this message to `rhs` in a classic three-way manner; must be
    // overridden by subclasses to compare all their fields
    virtual int compare(Machine* rhs) const = 0;
};

struct Diff final {
    // A diff simply captures the messages sent and delivered; only one message
    // is ever delivered for each diff, so no need for a vector there
    std::vector<Message*> sent;
    Message* delivered;
};

struct SystemState final {
    // Together, the messages and machines constitute the state of a system.
    std::vector<Message*> messages;
    std::vector<Machine*> machines;
    // States also store an ordered list of diffs, which constitute a method to
    // arrive at this state from the initial state
    std::vector<Diff> history;

    // Initialize with a machine list.
    SystemState(std::vector<Machine*> machines) : machines(machines) {}

    // When we explore the state graph, we deep copy the SystemState. This
    // copies the vectors of pointers, but does not copy the underlying machines
    // or messages. This should be fine, since messages are immutable and
    // machines must be manually copied when acted upon, which is handled by the
    // get_neighbors implementation.
    SystemState(const SystemState& other) {
        this->machines = other.machines;
        this->messages = other.messages;
    }

    // Returns a vector of neighboring states, with the added diffs to get
    // there. For now, a next state is reached by any machine accepting a
    // message from the queue.
    std::vector<SystemState> get_neighbors();

    // SystemStates are comparable so we can skip visited states; the history
    // is deliberately not included so states compare equal even if they have
    // a different history
    int compare(const SystemState& rhs) const;
    bool operator==(const SystemState& rhs) const {
        return !compare(rhs);
    }
    bool operator<(const SystemState& rhs) const {
        return compare(rhs) < 0;
    }

    // This is hard to do, since we don't really know which objects are still
    // being used; probably a good place for refcounting
    ~SystemState() {}
};

struct Invariant final {
    // An invariant is simply a named predicate over the state of a system.
    std::string name;
    std::function<bool(SystemState)> check;

    Invariant(std::string s, std::function<bool(SystemState)> fn)
        : name(s), check(fn) {}
};

struct Model final {
    // A model is a set of states on which we're doing a BFS, essentially.
    // It also has a set of invariants evaluated at each state, and a history
    // to arrive at each state.

    // Pending, visited are the usual BFS roles.
    std::queue<SystemState> pending;
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
