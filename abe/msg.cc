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
    }

    void handle_timer(uint64_t id) override {
        if (!ack) {
            uint64_t* msg = new uint64_t;
            *msg = val;
            send_message(dst, msg, 8);
            set_timer(0, 200);
        }
    }

    void handle_message(uint64_t src, void* data, size_t sz) override {
        if (src != dst) fail("Message came from wrong source!");
        if (sz != sizeof(int)) fail("Message has inappropriate length!");
        int* msg = (int*) data;
        if (*msg == 1) ack = true;
    }

    void print(int indent) override {
        iprintf(indent, "Acknowledged: %s value: %lu\n",
                ack ? "true" : "false", val);
    }
};

struct receiver : machine {
    bool recv;
    uint64_t val;

    void init() override {
        recv = false;
        val = 0;
    }

    void handle_timer(uint64_t id) override {}

    void handle_message(uint64_t src, void* data, size_t sz) override {
        if (sz != 8) fail("Message has inappropriate length!");
        uint64_t* msg = (uint64_t*) data;
        val = *msg;
        int* res = new int;
        *res = 1;
        send_message(src, res, sizeof(int));
    }

    void print(int indent) override {
        iprintf(indent, "Received: %s value: %lu\n",
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
    s->m.push_back(src);
    s->m.push_back(dst);
    return s;
}

bool predicate (state* s) {
    // TODO make a real predicate
    return true;
}
