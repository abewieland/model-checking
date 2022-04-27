#include <stdio.h>
#include <stdlib.h>
#include <set>

#include "simple.hh"

void msg::print(int indent) {
    iprintf(indent, "To: %lu From: %lu Size: %zu\n", dst, src, sz);
    iprintf(indent, "Data:");
    if (!data) {
        printf(" (null)");
    } else {
        size_t i;
        for (i = 0; i < 8 && i < sz; ++i) {
            printf(" %#02hhx", (uint8_t) *(((char*) data) + i));
        }
        if (i < sz) {
            printf("...");
        }
    }
    printf("\n");
}

void state::print(int indent) {
    iprintf(indent, "Machines:\n");
    for (auto it = m.begin(); it != m.end(); ++it) {
        machine* mch = *it;
        iprintf(indent + 2, "Messages:\n");
        for (auto mit = mch->queue.begin(); mit != mch->queue.end(); ++mit) {
            (*mit).print(indent + 4);
        }
        mch->print(indent + 2);
        printf("\n");
    }
}

void history::print(int indent) {
    iprintf(indent, "History:\n");
    for (auto it = prev.begin(); it != prev.end(); ++it) {
        (*it)->print(indent + 2);
        printf("\n");
    }
    iprintf(indent, "At:\n");
    curr->print(indent + 2);
}

[[noreturn]] void machine::fail(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static uint64_t time = 0;

void machine::set_timer(uint64_t id, uint64_t timeout) {
    msg m;
    m.src = id;
    m.dst = this->id;
    m.time = time + timeout;
    m.sz = 0;
    m.data = nullptr;
    queue.push_back(m);
}

void machine::send_message(uint64_t dst, void* data, size_t sz) {
    if (dst >= st->m.size()) fail("Invalid message destination");
    msg m;
    m.src = id;
    m.dst = dst;
    m.time = time;
    m.sz = sz;
    m.data = data;
    st->m[dst]->queue.push_back(m);
}

const uint64_t max_delay = 1000;

int main() {
    // for now, ignore issues of state space explosion and exhaustively check
    // every state (BFS)
    std::set<state*> visited;
    std::list<history> todo;
    history first;
    first.curr = init_state();
    for (auto it = first.curr->m.begin(); it != first.curr->m.end(); ++it) {
        (*it)->init();
    }
    todo.push_back(first);
    while (!todo.empty()) {
        history current = todo.front();
        current.print(0);
        // TODO stuff here!
        todo.pop_front();
    }
}
