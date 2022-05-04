#include "model.hpp"

std::vector<History> History::get_neighbors() {
    std::vector<History> results;
    for (size_t i = 0; i < curr.messages.size(); ++i) {
        // Each message may be delivered to make a new state
        Message* m = curr.messages[i];

        // The SystemState copy constructor copies the machine and message
        // vectors of pointers, but not their contents.
        SystemState next = SystemState(curr);

        // Since accepting a message may mutate state, clone the machine first;
        // if it didn't change, we'll delete it later
        next.messages.erase(next.messages.begin() + i);
        Machine* target = next.machines[m->dst]->clone();

        // This fresh machine object will handle the message, possibly emitting
        // new messages. These belong in the new message queue.
        std::vector<Message*> new_msg = target->handle_message(m);
        if (target->compare(next.machines[m->dst])) {
            next.machines[m->dst] = target;
        } else {
            delete target;
        }
        next.messages.insert(next.messages.end(),
                             new_msg.begin(), new_msg.end());

        // Build and append the history object.
        Diff d;
        d.delivered = m;
        d.sent = new_msg;
        History h{next};
        h.history = history;
        h.history.push_back(d);
        results.push_back(h);
    }
    return results;
}

int SystemState::compare(const SystemState& rhs) const {
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

// To construct a Model from an initial state and some invariants, run all of
// the machines' initialization tasks.
Model::Model(std::vector<Machine*> m, std::vector<Invariant> i)
    : invariants(i) {
    History h{SystemState{m}};

    // Initialize machines
    for (Machine*& m : h.curr.machines) {
        std::vector<Message*> new_msg = m->on_startup();
        h.curr.messages.insert(h.curr.messages.end(),
                               new_msg.begin(), new_msg.end());
    }

    // Visit the initial state first.
    pending.push(h);

    std::cout << "Initialized a new model with " << h.curr.machines.size()
              << " machines and " << invariants.size() << " invariants."
              << std::endl;
}

std::vector<SystemState> Model::run() {
    std::vector<SystemState> terminating;

    while (!this->pending.empty()) {
        History h = pending.front();
        pending.pop();

        // Note that we only care about the states we've visited, not how we got
        // there; since this is a BFS, the history should always be the minimum
        // possible
        visited.insert(h.curr);

        if (!check_invariants(h.curr)) {
            std::cerr << "Model error! Did not pass invariants." << std::endl;
        }

        std::vector<History> neighbors = h.get_neighbors();
        if (!neighbors.size()) terminating.push_back(h.curr);
        for (History& neighbor : neighbors) {
            if (visited.find(neighbor.curr) == visited.end()) {
                pending.push(neighbor);
            }
        }
    }
    return terminating;
}
