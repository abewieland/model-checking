#include <stdlib.h>
#include <stdio.h>

#include "simple.hh"

struct sender : machine {
    bool ack;
    uint64_t dst;
    uint64_t val;

    void init() override {
        ack = false;
        val = rand();
        uint64_t* msg = new uint64_t;
        *msg = val;
        send_message(dst, msg, 8);
        set_timer(0, 200);
        fprintf(stderr, "init called on sender!\n");
    }

    void handle_timer(uint64_t id) override {
        if (!ack) {
            uint64_t* msg = new uint64_t;
            *msg = val;
            send_message(dst, msg, 8);
            set_timer(0, 200);
        }
    }

    void handle_message(uint64_t src, size_t sz, void* data) override {
        if (src != dst) fail("Message came from wrong source!");
        if (sz != sizeof(int)) fail("Message has inappropriate length!");
        int* msg = (int*) data;
        if (*msg == 1) ack = true;
    }

    void print(int indent) override {
        iprintf(indent, "Message acknowledged: %s value: %lu\n",
                ack ? "true" : "false", val);
    }
};

struct receiver : machine {
    bool recv;
    uint64_t val;

    void init() override {
        recv = false;
        val = 0;
        fprintf(stderr, "init called on receiver!\n");
    }

    void handle_timer(uint64_t id) override {}

    void handle_message(uint64_t src, size_t sz, void* data) override {
        if (sz != 8) fail("Message has inappropriate length!");
        uint64_t* msg = (uint64_t*) data;
        val = *msg;
        int* res = new int;
        *res = 1;
        send_message(src, res, sizeof(int));
    }

    void print(int indent) override {
        iprintf(indent, "Message received: %s value: %lu\n",
                recv ? "true" : "false", val);
    }
};

state* init_state() {
    state* s = new state;
    sender* src = new sender;
    receiver* dst = new receiver;
    src->id = 0;
    src->st = s;
    dst->id = 1;
    src->st = s;
    src->dst = 1;
    s->m.reserve(2);
    s->m[src->id] = src;
    s->m[dst->id] = dst;
    return s;
}

bool predicate (state* s) {
    // TODO make a real predicate
    return true;
}
