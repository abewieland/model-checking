#include <vector>
#include <queue>
#include <set>
#include <string>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <typeinfo>

// Machine identifiers
typedef unsigned id_t;

struct RefCounter {
    // A simple reference counter

    RefCounter() : _refcount(1) {}
    virtual ~RefCounter() {}

    inline void ref_inc() {
        ++_refcount;
    }
    inline void ref_dec() {
        //if (!--_refcount) delete this;
    }

private:
    unsigned long _refcount;
};

struct Message : RefCounter {
    // A Message is the basic unit of communication in the model checker; they
    // are immutable and parameterized by a type field. If more data is needed
    // (like a payload), subclasses may be created (which must override
    // sub_compare)
    id_t src;
    id_t dst;
    int type;

    Message(id_t src, id_t dst, int type) : src(src), dst(dst), type(type) {}

    // Perform a three-way comparison of this message to `rhs`
    int compare(Message* rhs) const {
        if (int r = (int) src - rhs->src) return r;
        if (int r = (int) dst - rhs->dst) return r;
        return logical_compare(rhs);
    }

    // Similar, but ignore ids (for symmetry optimization)
    int logical_compare(Message* rhs) const {
        if (int r = type - rhs->type) return r;
        return sub_compare(rhs);
    }

    // Perform comparison on added fields in subclasses
    virtual int sub_compare(Message* rhs) const {
        return 0;
    }

    // Print out extra information about this message (extra fields, etc)
    // Please indent 4 spaces in this function
    virtual void sub_print() const {}
};

struct Machine : RefCounter {
    // A Machine is the base class for state machines in the system. Subclasses
    // may add mutable state. Like messages, they are parameterized by a type
    // field (to avoid using std::type_info). This is a purely abstract class,
    // since all machines must store some additional state
    id_t id;
    int type;

    Machine(id_t id, int type) : id(id), type(type) {}

    // A machine must be cloneable to allow for mutation. Subclasses must
    // implement this method such that `compare(clone()) == 0`.
    virtual Machine* clone() const = 0;

    // Perform a three-way comparison of this machine to `rhs`
    int compare(Machine* rhs) const {
        if (int r = (int) id - rhs->id) return r;
        return logical_compare(rhs);
    }

    // Similar, but ignore id (for symmetry optimization)
    int logical_compare(Machine* rhs) const {
        if (int r = type - rhs->type) return r;
        return sub_compare(rhs);
    }

    // Perform comparison on added fields in subclasses
    virtual int sub_compare(Machine* rhs) const = 0;

    // On startup a machine might manipulate its own state, then return a vector
    // of messages it emits on initialization.
    virtual std::vector<Message*> on_startup() {
        return std::vector<Message*>{};
    }

    // Machines react to receiving messages by:
    //  (1) Updating their state accordingly
    //  (2) Returning a vector of new messages to emit in response
    // After initialization, this is the main handler.
    virtual std::vector<Message*> handle_message(Message* msg) {
        return std::vector<Message*>{};
    }
};

struct Diff : RefCounter {
    // A Diff captures the change between two states, which may only be caused
    // by a message being delivered or dropped (and if a message is delivered,
    // more may be sent)
    std::vector<Message*> sent;
    Message* delivered;
    Message* dropped;

    Diff() : delivered(nullptr), dropped(nullptr) {}

    ~Diff() {
        for (Message*& m : sent) m->ref_dec();
        if (delivered) delivered->ref_dec();
        if (dropped) dropped->ref_dec();
    }
};

struct SystemState final {
    // Together, messages and machines constitute the state of a system
    std::vector<Message*> messages;
    std::vector<Machine*> machines;
    // States also store an ordered list of diffs, which constitute a method to
    // arrive at this state from the initial state
    std::vector<Diff*> history;

    // 1 + the prececessor's depth
    int depth;

    // Initialize with a machine list.
    SystemState(std::vector<Machine*> machines)
        : machines(machines), depth(0) {}

    // When we explore the state graph, we deep copy the SystemState. This
    // copies the vectors of pointers, but does not copy the underlying machines
    // or messages. This should be fine, since messages are immutable and
    // machines must be manually copied when acted upon, which is handled by the
    // get_neighbors implementation.
    SystemState(const SystemState& rhs) {
        messages = rhs.messages;
        for (Message*& m : messages) m->ref_inc();
        machines = rhs.machines;
        for (Machine*& m : machines) m->ref_inc();
        history = rhs.history;
        for (Diff*& d : history) d->ref_inc();
        depth = rhs.depth;
    }

    // Print a trace of what transpired
    void print_history() const {
        fprintf(stderr, "History stack trace:\n");
        for (Diff* const& d : history) {
            if (d->delivered) {
                printf("Message from %u (type %d) delivered to %u\n",
                       d->delivered->src, d->delivered->type, d->delivered->dst);
                d->delivered->sub_print();
            }
            if (d->dropped) {
                printf("Message from %u (type %d) dropped\n",
                       d->dropped->src, d->dropped->type);
                d->dropped->sub_print();
            }
        }
    }

    // SystemStates are comparable so we can skip visited states; the history
    // is deliberately not included so states compare equal even if they have
    // a different history
    int compare(const SystemState* rhs) const {
        if (long r = (long) messages.size() - rhs->messages.size()) return r;
        for (size_t i = 0; i < messages.size(); ++i) {
            if (int r = messages[i]->compare(rhs->messages[i])) return r;
        }
        if (long r = (long) machines.size() - rhs->machines.size()) return r;
        for (size_t i = 0; i < machines.size(); ++i) {
            if (int r = machines[i]->compare(rhs->machines[i])) return r;
        }
        return 0;
    }
    bool operator==(const SystemState& rhs) const {
        return !compare(&rhs);
    }
    bool operator<(const SystemState& rhs) const {
        return compare(&rhs) < 0;
    }

    // The builtin destructor will destroy the vectors, but we have to decrement
    // all their counters (since they're pointers that may be shared)
    ~SystemState() {
        for (Message*& m : messages) m->ref_dec();
        for (Machine*& m : machines) m->ref_dec();
        for (Diff*& d : history) d->ref_dec();
    }
};

struct Predicate final {
    // Named predicates over system states
    const char* name;
    std::function<bool(const SystemState&)> check;

    Predicate(const char* s, std::function<bool(const SystemState&)> fn)
        : name(s), check(fn) {}
};

struct Model final {
    // A model is a set of states on which we're doing a BFS, essentially.
    // It also has a set of invariants evaluated at each state, and a history
    // to arrive at each state.
    std::vector<SystemState> pending;
    std::set<SystemState> visited;
    std::vector<Predicate> invariants;

    // Initialize a model with an initial state (a vector of machines) and
    // possibly invariants
    Model(std::vector<Machine*> m,
          std::vector<Predicate> i = std::vector<Predicate>{});

    // Model check until a maximum depth (-1 for indefinitely). If `max_depth`
    // is non-negative, checking stops at that depth and all pending states
    // are returned. Otherwise, model checking continues until all new states
    // have been visited, and a list of terminating states is returned. If
    // `exclude_symmetries` is true, use the symmetry removing optimization.
    // If `interesting_states` has members, check every state against the list
    // and start over from any state which matches
    std::set<SystemState> run(int max_depth = -1,
        bool exclude_symmetries = true,
        std::vector<Predicate> interesting_states = std::vector<Predicate>{});
};
