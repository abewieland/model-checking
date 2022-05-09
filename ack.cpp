#include <time.h>
#include "model.hpp"

// A simple example of two machines, one continuously sending a value to the
// other until it responds

#define MSG_TMR 1
#define MSG_ACK 2
#define MSG_VAL 3

#define MCH_SND 1
#define MCH_RCV 2

struct Val : Message {
    int val;
    Val(id_t src, id_t dst, int val) : Message(src, dst, MSG_VAL), val(val) {}

    int sub_compare(Message* rhs) const override {
        return val - dynamic_cast<Val*>(rhs)->val;
    }

    void sub_print() const override {
        printf("    Value %d\n", val);
    }
};

struct Sender : Machine {
    id_t dst;
    int val;
    bool ack;

    Sender(id_t id, id_t dst, int val)
        : Machine(id, MCH_SND), dst(dst), val(val), ack(false) {}

    Sender* clone() const override {
        Sender* s = new Sender(id, dst, val);
        s->ack = ack;
        return s;
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        switch (m->type) {
            case MSG_TMR:
                if (!ack) {
                    ret.push_back(new Val(id, dst, val));
                    ret.push_back(new Message(id, id, MSG_TMR));
                }
                break;
            case MSG_ACK:
                ack = true;
                break;
        }
        return ret;
    }

    std::vector<Message*> on_startup() override {
        #ifdef B
        ack = true;
        #endif
        std::vector<Message*> ret;
        ret.push_back(new Val(id, dst, val));
        return ret;
    }

    int sub_compare(Machine* rhs) const override {
        Sender* m = dynamic_cast<Sender*>(rhs);
        if (int r = val - m->val) return r;
        return ack - m->ack;
    }
};

struct Receiver : Machine {
    int val;
    bool recv;

    Receiver(id_t id) : Machine(id, MCH_RCV), val(-1), recv(false) {}

    Receiver* clone() const override {
        Receiver* r = new Receiver(id);
        r->val = val;
        r->recv = recv;
        return r;
    }

    std::vector<Message*> handle_message(Message* m) override {
        // Pretty simple for the receiver - send acknowledgment
        std::vector<Message*> ret;
        val = dynamic_cast<Val*>(m)->val;
        ret.push_back(new Message(id, m->src, MSG_ACK));
        return ret;
    }

    int sub_compare(Machine* rhs) const override {
        Receiver* m = dynamic_cast<Receiver*>(rhs);
        if (int r = val - m->val) return r;
        return recv - m->recv;
    }
};

bool invariant(const SystemState& st) {
    Sender* s = dynamic_cast<Sender*>(st.machines[0]);
    Receiver* r = dynamic_cast<Receiver*>(st.machines[1]);
    if (r->recv && r->val != s->val) return false;
    if (s->ack && r->val != s->val) return false;
    return true;
}

int main() {
    srand(time(0));
    std::vector<Machine*> m;
    m.push_back(new Sender(0, 1, rand()));
    m.push_back(new Receiver(1));
    std::vector<Predicate> i;
    i.push_back(Predicate{"Consistency", invariant});
    Model model{m, i};

    std::set<SystemState> res = model.run();
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
