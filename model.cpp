#include "model.hpp"

struct LogicalMachine {
    Machine* m;
    std::vector<Message*> outgoing;
    std::vector<Message*> incoming;

    LogicalMachine (Machine* m) : m(m) {
        m->ref_inc();
    }
    ~LogicalMachine() {
        m->ref_dec();
        for (Message*& m : outgoing) m->ref_dec();
        for (Message*& m : incoming) m->ref_dec();
    }

    int compare(const LogicalMachine* rhs) const {
        if (int r = m->logical_compare(rhs->m)) return r;
        if (long r = (long) outgoing.size() - rhs->outgoing.size()) return r;
        if (long r = (long) incoming.size() - rhs->incoming.size()) return r;
        for (size_t i = 0; i < outgoing.size(); ++i) {
            if (int r = outgoing[i]->logical_compare(rhs->outgoing[i])) return r;
        }
        for (size_t i = 0; i < incoming.size(); ++i) {
            if (int r = incoming[i]->logical_compare(rhs->incoming[i])) return r;
        }
        return 0;
    }

    bool operator<(const LogicalMachine& rhs) const {
        return compare(&rhs) < 0;
    }
};

struct LogicalState {
    std::vector<LogicalMachine> machines;

    LogicalState() {}

    // Copy construct from a (normal) state
    LogicalState(SystemState& s) {
        for (Machine*& m : s.machines) {
            machines.emplace_back(m);
        }

        for (Message*& m : s.messages) {
            machines[m->src].outgoing.push_back(m);
            m->ref_inc();
            machines[m->dst].incoming.push_back(m);
            m->ref_inc();
        }

        for (LogicalMachine& m : machines) {
            auto lambda = [] (Message* a, Message* b) {
                return a->logical_compare(b);
            };
            std::sort(m.outgoing.begin(), m.outgoing.end(), lambda);
            std::sort(m.incoming.begin(), m.incoming.end(), lambda);
        }

        std::sort(machines.begin(), machines.end());
    }

    bool operator<(const LogicalState& rhs) const {
        return machines < rhs.machines;
    }
};

std::vector<SystemState> get_all_neighbors(std::vector<SystemState>& nodes,
                                           bool exclude_symmetries,
                                           std::set<SystemState>& terminating,
                                           std::set<SystemState>& visited) {
    std::set<LogicalState> logical_states;
    std::vector<SystemState> ret;
    LogicalState empty; // ie, with no machines

    for (const SystemState& n : nodes) {
        for (size_t i = 0; i < n.messages.size(); ++i) {
            // Each message may be delivered to make a new state
            Diff* d = new Diff();
            d->delivered = n.messages[i];
            // no need to increment the refcount on d->delivered, because while
            // d will now own it, next will no longer

            // The SystemState copy constructor copies the machine and message
            // vectors of pointers, but not their contents.
            SystemState next = SystemState(n);
            next.depth = n.depth + 1;

            // Since accepting a message may mutate state, clone the machine first;
            // if it didn't change, we'll delete it later
            next.messages.erase(next.messages.begin() + i);
            Machine* target = next.machines[d->delivered->dst]->clone();

            // This fresh machine object will handle the message, possibly emitting
            // new messages. These belong in the new message queue.
            d->sent = target->handle_message(d->delivered);

            if (target->compare(next.machines[d->delivered->dst])) {
                next.machines[d->delivered->dst]->ref_dec();
                next.machines[d->delivered->dst] = target;
            } else {
                // This should delete it, as it only belonged to this scope
                target->ref_dec();
            }

            LogicalState ls = exclude_symmetries ? LogicalState(next) : empty;
            // If this is a new state, add it to the list
            if (visited.find(next) == visited.end()
                && (!exclude_symmetries
                    || logical_states.find(ls) == logical_states.end())) {
                for (Message*& m : d->sent) {
                    next.messages.push_back(m);
                    m->ref_inc();
                }
                next.history.push_back(d);
                ret.push_back(next);

                if (exclude_symmetries) {
                    logical_states.insert(ls);
                }
            } else {
                d->ref_dec();
            }
        }
        if (!n.messages.size()) terminating.insert(n);
    }
    return ret;
}

// To construct a Model from an initial state and some invariants, run all of
// the machines' initialization tasks.
Model::Model(std::vector<Machine*> m, std::vector<Predicate> i) : invariants(i) {
    SystemState s{m};

    // Initialize machines
    for (Machine*& m : s.machines) {
        std::vector<Message*> new_msg = m->on_startup();
        s.messages.insert(s.messages.end(), new_msg.begin(), new_msg.end());
    }

    // Visit the initial state first.
    pending.push_back(s);

    printf("Initialized a new model with %lu machines and %lu invariants.\n",
           s.machines.size(), invariants.size());
}

std::set<SystemState> Model::run(int max_depth, bool exclude_symmetries,
                                 std::vector<Predicate> interesting_states,
                                 bool print) {
    std::set<SystemState> terminating;
    int depth = 0;
    int nodes_seen = 0;

    while ((max_depth < 0 || depth <= max_depth) && !pending.empty()) {
        if (print) {
            printf("Depth searched: %d\n    Total nodes explored: %d\n"
                   "    Unique nodes visited: %lu\n    Frontier size: %lu\n",
                   depth, nodes_seen, visited.size(), pending.size());
            printf("    Sample queue length: %lu\n", pending[0].messages.size());
            printf("    Terminating states found: %lu\n", terminating.size());
        }

        for (const SystemState& s : pending) {
            ++nodes_seen;

            // Note that we only care about the states we've visited, not how we
            // got there; since this is a BFS, the history should always be the
            // most minimal possible
            visited.insert(s);

            // Ensure that `s` validates against all invariants
            for (const Predicate& p : invariants) {
                if (!p.match(s)) {
                    printf("INVARIANT VIOLATED: %s\n", p.name);
                    s.print_history();
                    exit(1);
                }
            }

            // Guided search; if a state matches any of the `interesting`
            // predicates, start over from that state
            for (const Predicate& p : interesting_states) {
                if (p.match(s)) {
                    printf("INTERESTING STATE FOUND: %s\n", p.name);
                    pending.clear();
                    pending.push_back(s);
                    break;
                }
            }
        }
        pending = get_all_neighbors(pending, exclude_symmetries,
                                    terminating, visited);
        ++depth;
    }
    return terminating;
}
