#include <stdlib.h>
#include <time.h>
#include "model.hpp"

// A simple example of two machines, one continuously sending a value to the
// other until it responds

#define MSG_TIMER 1
#define MSG_ACK   2
#define MSG_VAL   3

struct Val : Message {
    int val;
    Val(id_t src, id_t dst, int val) : Message(src, dst, MSG_VAL), val(val) {}

    int sub_compare(Message* rhs) const override {
        return val - dynamic_cast<Val*>(rhs)->val;
    }
};

struct Sender : Machine {
    id_t dst;
    int val;
    bool ack;

    Sender(id_t id, id_t dst, int val)
        : Machine(id), dst(dst), val(val), ack(false) {}

    Sender* clone() const override {
        Sender* s = new Sender(id, dst, val);
        s->ack = ack;
        return s;
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        switch (m->type) {
            case MSG_TIMER:
                if (!ack) {
                    ret.push_back(new Val(id, dst, val));
                    ret.push_back(new Message(id, id, MSG_TIMER));
                }
                break;
            case MSG_ACK:
                ack = true;
                break;
            default:
                // Maybe throw an error here later or something
                break;
        }
        return ret;
    }

    std::vector<Message*> on_startup() override {
        std::vector<Message*> ret;
        ret.push_back(new Val(id, dst, val));
        return ret;
    }

    int compare(Machine* rhs) const override {
        if (int r = (int) id - rhs->id) return r;
        Sender* m = dynamic_cast<Sender*>(rhs);
        if (int r = val - m->val) return r;
        return ack - m->ack;
    }
};

struct Receiver : Machine {
    int val;
    bool recv;

    Receiver(id_t id) : Machine(id), val(-1), recv(false) {}

    Receiver* clone() const override {
        Receiver* r = new Receiver(id);
        r->val = val;
        r->recv = recv;
        return r;
    }

    std::vector<Message*> handle_message(Message* m) override {
        // Pretty simple for the receiver - send acknowledgment
        std::vector<Message*> ret;
        if (m->type != MSG_VAL) {
            // Again, like throw an error
        }
        val = dynamic_cast<Val*>(m)->val;
        ret.push_back(new Message(id, m->src, MSG_ACK));
        return ret;
    }

    int compare(Machine* rhs) const override {
        if (int r = (int) id - rhs->id) return r;
        Receiver* m = dynamic_cast<Receiver*>(rhs);
        if (int r = val - m->val) return r;
        return recv - m->recv;
    }
};

int main() {
    srand(time(0));
    std::vector<Machine*> m;
    m.push_back(new Sender(0, 1, rand()));
    m.push_back(new Receiver(1));
    Model model{m, std::vector<Invariant>{}};

    std::vector<SystemState> res = model.run();
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
