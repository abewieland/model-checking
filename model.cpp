#include "model.hpp"

std::vector<SystemState> SystemState::get_neighbors() {
    std::vector<SystemState> results;
    for (size_t i = 0; i < messages.size(); ++i) {
        // Each message may be delivered to make a new state
        Diff* d = new Diff();
        d->delivered = messages[i];
        // no need to increment the refcount on d->delivered, because while d
        // will now own it, next will no longer own it

        // The SystemState copy constructor copies the machine and message
        // vectors of pointers, but not their contents.
        SystemState next = SystemState(*this);

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
        for (Message*& m : d->sent) {
            m->ref_inc();
            next.messages.push_back(m);
        }
        next.history.push_back(d);
        results.push_back(next);
    }
    return results;
}

int SystemState::compare(const SystemState& rhs) const {
    // As noted in model.hpp, ignore history
    if (long r = messages.size() - rhs.messages.size()) return r;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (int r = messages[i]->compare(rhs.messages[i])) return r;
    }
    if (long r = machines.size() - rhs.machines.size()) return r;
    for (size_t i = 0; i < machines.size(); ++i) {
        if (int r = machines[i]->compare(rhs.machines[i])) return r;
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

std::vector<SystemState> Model::run() {
    std::vector<SystemState> terminating;

    while (!this->pending.empty()) {
        SystemState s = pending.front();
        pending.pop();

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

        // And add its pending members, if there are any
        std::vector<SystemState> neighbors = s.get_neighbors();
        if (!neighbors.size()) terminating.push_back(s);
        for (SystemState& neighbor : neighbors) {
            if (visited.find(neighbor) == visited.end()) {
                pending.push(neighbor);
            }/* else {
                // Don't know if this is legal - can you edit the vector in place?
                delete &neighbor;
            }*/
        }
    }
    return terminating;
}
