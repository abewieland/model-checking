#include "model.hpp"

struct LogicalMachine {
    Machine* m;
    std::vector<Message*> outgoing;
    std::vector<Message*> incoming;

    LogicalMachine (Machine* m) : m(m) {
        m->ref_inc();
    }

    // We have to override the default copy constructor to ensure the refcounts
    // are correct
    LogicalMachine (const LogicalMachine& rhs) {
        m = rhs.m;
        m->ref_inc();
        outgoing = rhs.outgoing;
        for (Message*& m : outgoing) m->ref_inc();
        incoming = rhs.incoming;
        for (Message*& m : incoming) m->ref_inc();
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

    // Construct from a (normal) state
    LogicalState(SystemState& s) {
        for (Machine*& m : s.machines) {
            // avoid an extra copy construction via emplacement
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
                return a->logical_compare(b) < 0;
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

#define in_set(set, val) ((set).find(val) != (set).end())

std::vector<SystemState> get_all_neighbors(std::vector<SystemState>& nodes,
                                           bool exclude_symmetries,
                                           std::set<SystemState>& terminating,
                                           std::set<SystemState>& visited) {
    std::set<LogicalState> logical_states;
    std::vector<SystemState> ret;

    for (const SystemState& n : nodes) {
        for (size_t i = 0; i < n.messages.size(); ++i) {
            // Each message may be delivered or dropped to make a new state
            Diff* del = new Diff();
            Diff* drop = new Diff();
            del->delivered = n.messages[i];
            drop->dropped = n.messages[i];
            // no need to increment these refcounts, because while they now own
            // them, their next states won't

            SystemState next_del = SystemState(n);
            next_del.depth = n.depth + 1;
            next_del.messages.erase(next_del.messages.begin() + i);
            SystemState next_drop = SystemState(n);
            next_drop.depth = n.depth + 1;
            next_drop.messages.erase(next_drop.messages.begin() + i);

            // Since accepting a message may mutate state, clone the machine
            // first; if it didn't change, we'll delete it later
            Machine* target = next_del.machines[del->delivered->dst]->clone();

            // This fresh machine object will handle the message, possibly
            // emitting new messages. These belong in the new message queue.
            del->sent = target->handle_message(del->delivered);

            if (target->compare(next_del.machines[del->delivered->dst])) {
                next_del.machines[del->delivered->dst]->ref_dec();
                next_del.machines[del->delivered->dst] = target;
            } else {
                // This should delete it, as it only belonged to this scope
                target->ref_dec();
            }

            // Add the new messages to the queue
            for (Message*& m : del->sent) {
                next_del.messages.push_back(m);
                m->ref_inc();
            }

            // And if this is a new state, add it to the list
            if (exclude_symmetries) {
                LogicalState ldel{next_del};
                LogicalState ldrop{next_drop};
                if (!in_set(visited, next_del)
                    && !in_set(logical_states, ldel)) {
                    next_del.history.push_back(del);
                    ret.push_back(next_del);
                    logical_states.insert(ldel);
                } else {
                    del->ref_dec();
                }
                if (!in_set(visited, next_drop) && drop->dropped->may_drop
                    && !in_set(logical_states, ldrop)) {
                    next_drop.history.push_back(drop);
                    ret.push_back(next_drop);
                    logical_states.insert(ldrop);
                } else {
                    drop->ref_dec();
                }
            } else {
                if (!in_set(visited, next_del)) {
                    next_del.history.push_back(del);
                    ret.push_back(next_del);
                } else {
                    del->ref_dec();
                }
                if (!in_set(visited, next_drop) && drop->dropped->may_drop) {
                    next_drop.history.push_back(drop);
                    ret.push_back(next_drop);
                } else {
                    drop->ref_dec();
                }
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

    // All models have error handling invariants
    invariants.emplace_back("Valid messages", [] (const SystemState& s) {
        for (Machine* const& m : s.machines) {
            if (m->error == ERR_BADMSG) return false;
        }
        return true;
    });

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
