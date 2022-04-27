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
    iprintf(indent, "Time: %lu - Machines:\n", time);
    for (auto it = m.begin(); it != m.end(); ++it) {
        machine* mch = *it;
        iprintf(indent + 2, "Id %lu - Messages:\n", mch->id);
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

const uint64_t max_delay = 1000;
static history current;

void machine::set_timer(uint64_t id, uint64_t timeout) {
    msg m;
    m.src = id;
    m.dst = this->id;
    m.sz = st->time + timeout;
    m.data = nullptr;
    queue.push_back(m);
}

void machine::send_message(uint64_t dst, void* data, size_t sz) {
    if (dst >= st->m.size()) fail("Invalid message destination");
    msg m;
    m.src = id;
    m.dst = dst;
    m.sz = sz;
    m.data = data;
    st->m[dst]->queue.push_back(m);
}

[[noreturn]] void machine::fail(const char* msg) {
    printf("%s\n", msg);
    current.print(0);
    exit(1);
}

struct compare_state {
    // just ignore the time variable
    bool operator() (state* a, state* b) {
        return std::less<std::vector<machine*>>{}(a->m, b->m);
    }
};

int main() {
    // for now, ignore issues of state space explosion and exhaustively check
    // every state (BFS)
    std::set<state*, compare_state> visited;
    std::list<history> todo;
    history first;
    first.curr = init_state();
    for (auto it = first.curr->m.begin(); it != first.curr->m.end(); ++it) {
        (*it)->init();
    }
    todo.push_back(first);
    while (!todo.empty()) {
        current = todo.front();
        // first, ensure we haven't already seen this state, and then check the
        // predicate
        if (visited.find(current.curr) != visited.end()) {
            todo.pop_front();
            continue;
        }
        if (!predicate(current.curr)) {
            printf("Predicate failed!\n");
            current.print(0);
            exit(1);
        }
        // a global event list would also probably be better than this mess
        // find next timer event (which occur on their schedule)
        uint64_t next_time = -1;
        for (auto mit = current.curr->m.begin(); mit != current.curr->m.end(); ++mit) {
            machine* mn = *mit;
            for (auto it = mn->queue.begin(); it != mn->queue.end(); ++it) {
                msg& m = *it;
                if (!m.data && m.sz < next_time) {
                    next_time = m.sz;
                }
            }
        }
        if (next_time < (uint64_t) -1) current.curr->time = next_time;
        // seems bad to loop twice, but whatever - now make a new state for each
        // possible timer or message to deliver
        for (auto mit = current.curr->m.begin(); mit != current.curr->m.end(); ++mit) {
            machine* mn = *mit;
            for (auto it = mn->queue.begin(); it != mn->queue.end(); ++it) {
                msg& m = *it;
                if (m.data || m.sz <= current.curr->time) {
                    msg to_deliver;
                    state* next_s = new state;
                    for (auto mit_ = current.curr->m.begin();
                         mit_ != current.curr->m.end(); ++mit_) {
                        if (mit_ == mit) {
                           // then (selectively) copy the message queue
                           for (auto it_ = mn->queue.begin();
                                it_ != mn->queue.end(); ++it_) {
                                    if (it_ == it) {
                                        // save the message/timer to be delivered,
                                        // which will be handled later
                                        to_deliver = *it_;
                                    } else {
                                        // I hope this copy constructs
                                        next_s->m.back()->queue.push_back(*it_);
                                    }
                                }
                        } else {
                            // again, I sure hope this copy constructs
                            next_s->m.push_back(*mit_);
                        }
                    }
                    history next;
                    next.prev.insert(next.prev.begin(), current.prev.begin(),
                                     current.prev.end());
                    next.prev.push_back(current.curr);
                    next.curr = next_s;
                    todo.push_back(next);
                    // now, actually deliver the message, but temporarily switch
                    // to next so that machine::fail will print the correct
                    // stack trace
                    history temp = current;
                    current = next;
                    // TODO dst is null; not sure why - maybe copy construction
                    // didn't work fully as planned
                    machine* dst = current.curr->m[to_deliver.dst];
                    if (to_deliver.data) {
                        dst->handle_message(to_deliver.src, to_deliver.data,
                                            to_deliver.sz);
                    } else {
                        dst->handle_timer(to_deliver.src);
                    }
                    current = temp;
                }
            }
        }
        todo.pop_front();
    }
}
