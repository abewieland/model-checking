#include "model.hpp"

bool check_sym(Symmetry& checker, SystemState& curr, std::vector<SystemState>& so_far) {
    // true if symmetric with anything in so_far
    for(auto s : so_far) {
        if(checker(curr, s)) {
            return true;
        }
    }
    return false;
}

bool is_novel_state(std::vector<Symmetry>& checkers, SystemState& curr, std::vector<SystemState>& so_far) {
    for(auto checker : checkers) {
        if(check_sym(checker, curr, so_far)) {
            return false;
        }
    }
    return true;
}

struct CanonicalizedStateEntry {
    Machine * m;
    std::vector<Message*> outgoing;
    std::vector<Message*> incoming;

    CanonicalizedStateEntry (Machine *m) : m(m) {}

    int compare(const CanonicalizedStateEntry& rhs) const {
        if (int r = m->compare_with_type_info(rhs.m)) return r;

        if (long r = outgoing.size() - rhs.outgoing.size()) return r;

        if (long r = incoming.size() - rhs.incoming.size()) return r;

        for (size_t i = 0; i < outgoing.size(); ++i) {
            if (int r = outgoing[i]->compare_no_src_dst(rhs.outgoing[i])) return r;
        }
        for (size_t i = 0; i < incoming.size(); ++i) {
            if (int r = incoming[i]->compare_no_src_dst(rhs.incoming[i])) return r;
        }
        return 0;
    }

    bool operator<(const CanonicalizedStateEntry& rhs) const {
        return compare(rhs) < 0;
    }
};

struct CanonicalizedState {

    // machine, outgoing, incoming
    std::vector<CanonicalizedStateEntry> contents;

    CanonicalizedState(SystemState& s) {
        for(auto& m : s.machines) {
            CanonicalizedStateEntry n(m->clone());
            contents.push_back(n);
            m->ref_inc();
        }

        for(Message*& m : s.messages) {
            id_t src = m->src;
            id_t dst = m->dst;
            contents[src].outgoing.push_back(m);
            contents[dst].incoming.push_back(m);
        }

        for(auto& t : contents) {
            t.m->id = 0;
            std::sort(t.outgoing.begin(), t.outgoing.end());
            std::sort(t.incoming.begin(), t.incoming.end());
        }

        std::sort(contents.begin(), contents.end());
    }

    bool operator<(const CanonicalizedState& rhs) const {
        return contents < rhs.contents;
    }
};


void SystemState::get_neighbors(std::vector<SystemState>& results, std::vector<Symmetry>& symmetries, std::set<SystemState>& visited) {
    // std::vector<SystemState> results;
    results.clear();
    results.reserve(messages.size());

    std::set<CanonicalizedState> canonical_states;

    for (size_t i = 0; i < messages.size(); ++i) {
        // Each message may be delivered to make a new state
        Diff* d = new Diff();
        d->delivered = messages[i];
        // no need to increment the refcount on d->delivered, because while d
        // will now own it, next will no longer own it

        // The SystemState copy constructor copies the machine and message
        // vectors of pointers, but not their contents.
        SystemState next = SystemState(*this);
        next.depth = this->depth + 1;

        // Since accepting a message may mutate state, clone the machine first;
        // if it didn't change, we'll delete it later
        next.messages.erase(next.messages.begin() + i);
        Machine* target = next.machines[d->delivered->dst]->clone();

        // This fresh machine object will handle the message, possibly emitting
        // new messages. These belong in the new message queue.
        d->sent = target->handle_message(d->delivered);

        if (target->compare(next.machines[d->delivered->dst])) {
            next.machines[d->delivered->dst] = target;
        } else {
            // This should delete it, as it only belonged to this scope
            target->ref_dec();
        }

        CanonicalizedState can_ver = CanonicalizedState(next);

        if((canonical_states.find(can_ver) == canonical_states.end())) {

            for (Message*& m : d->sent) {
                m->ref_inc();
                next.messages.push_back(m);
            }
            next.history.push_back(d);
            results.push_back(next);
            canonical_states.insert(can_ver);

        } else {
            target->ref_dec();
        }
    }
    // return results;
}

int SystemState::compare(const SystemState& rhs) const {
    // As noted in model.hpp, ignore history
    if (long r = messages.size() - rhs.messages.size()) return r;
    if (long r = machines.size() - rhs.machines.size()) return r;
    for (size_t i = 0; i < machines.size(); ++i) {
        if (int r = machines[i]->compare(rhs.machines[i])) return r;
    }
    for (size_t i = 0; i < messages.size(); ++i) {
        if (int r = messages[i]->compare(rhs.messages[i])) return r;
    }

    return 0;
}

void SystemState::print_history() const {
    // We'll probably eventually want to also print the initial state, but meh
    fprintf(stderr, "History stack trace:\n");
    for (Diff* const& d : history) {
        // We also probably want the messages to have virtual printing functions
        // (machines too), so that more data can be provided
        fprintf(stderr, "Message from %u (type %d) delivered to %u\n",
                d->delivered->src, d->delivered->type, d->delivered->dst);
    }
}

// To construct a Model from an initial state and some invariants, run all of
// the machines' initialization tasks.
Model::Model(std::vector<Machine*> m, std::vector<Invariant> i)
    : invariants(i) {
    SystemState s{m};

    // Initialize machines
    for (Machine*& m : s.machines) {
        std::vector<Message*> new_msg = m->on_startup();
        s.messages.insert(s.messages.end(), new_msg.begin(), new_msg.end());
    }

    // Visit the initial state first.
    pending.push(s);

    printf("Initialized a new model with %lu machines and %lu invariants.\n",
           s.machines.size(), invariants.size());
}

std::vector<SystemState> Model::run(int max_depth) {
    // std::vector<SystemState> terminating;
    std::set<SystemState> terminating;

    std::vector<SystemState> neighbors;

    int max_depth_seen = 0;

    int nodes_explored = 0;
    while (!this->pending.empty()) {
        SystemState s = pending.front();
        pending.pop();
        nodes_explored++;
        // Note that we only care about the states we've visited, not how we got
        // there; since this is a BFS, the history should always be the minimum
        // possible
        visited.insert(s);

        // Ensure that `s` validates against all invariants
        for (const Invariant& i : invariants) {
            if (!i.check(s)) {
                fprintf(stderr, "INVARIANT VIOLATED: %s\n", i.name);
                s.print_history();
                exit(1);
            }
        }

        if(max_depth != -1 && s.depth >= max_depth) {
            terminating.insert(s);
            continue;
        }

        // And add its pending members, if there are any
        s.get_neighbors(neighbors, this->symmetries, visited);

        if (!neighbors.size()) terminating.insert(s);


        for (SystemState& neighbor : neighbors) {
            if (visited.find(neighbor) == visited.end()) {
                pending.push(neighbor);
            }
        }

        if(s.depth > max_depth_seen) {
            max_depth_seen = s.depth;
            printf("Depth searched: %d\n Total nodes explored: %d\n Unique nodes visited: %lu\n Frontier size: %lu\n", max_depth_seen, nodes_explored, visited.size(), pending.size());
        }
    }
    std::vector<SystemState> v(terminating.begin(), terminating.end());
    return v;
}

std::vector<SystemState> Model::run() {
    return run(-1);
}
