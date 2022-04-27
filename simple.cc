#include <stdio.h>
#include <set>

#include "simple.hh"

void state::print(int indent) {
    printf("%*sMachines:\n", indent, "");
    for (auto it = m.begin(); it != m.end(); ++it) {
        (*it)->print(indent + 2);
        printf("\n");
    }
}

void history::print(int indent) {
    printf("%*sHistory:\n", indent, "");
    for (auto it = prev.begin(); it != prev.end(); ++it) {
        (*it)->print(indent + 2);
        printf("\n");
    }
    printf("%*sAt:\n", indent, "");
    curr->print(indent + 2);
}

const uint64_t max_delay = 1000;
static uint64_t time = 0;

int main() {
    // for now, ignore issues of state space explosion and exhaustively check
    // every state (BFS)
    std::set<state*> visited;
    std::list<history> todo;
    history first;
    first.curr = init_state();
    todo.push_back(first);
    while (!todo.empty()) {
        history current = todo.front();
        // TODO stuff here!
        todo.pop_front();
    }
}
