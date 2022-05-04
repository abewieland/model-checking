#include <stdlib.h>
#include <time.h>
#include "model.hpp"

// A simple example of two machines, one continuously sending a value to the
// other until it responds

struct Timer : Message {
    using Message::Message;
};

struct Ack : Message {
    using Message::Message;
};

struct Msg : Message {
    int val;
    Msg(int src, int dst, int val) : Message(src, dst), val(val) {}
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
        // Because of cyclic reference issues (I can't use dynamic_cast in
        // Message::operate unless it's a full type, nor can I do typeid here
        // unless it's a full type), I decided to just put all the functionality
        // here
        std::vector<Message*> ret;
        // The type checking stuff turns out to be fairly easy to write, though
        // I hate to think of what is occurring behind the scenes
        if (typeid(*m) == typeid(Timer)) {
            if (!ack) {
                ret.push_back(new Msg(id, dst, val));
                ret.push_back(new Timer(id, id));
            }
        } else if (typeid(*m) == typeid(Ack)) {
            ack = true;
        }
        return ret;
    }

    std::vector<Message*> on_startup() override {
        std::vector<Message*> ret;
        ret.push_back(new Msg(id, dst, val));
        return ret;
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
        val = dynamic_cast<Msg*>(m)->val;
        ret.push_back(new Ack(id, m->src));
        return ret;
    }
};

int main() {
    srand(time(0));
    std::vector<Machine*> m;
    m.push_back(new Sender(0, 1, rand()));
    m.push_back(new Receiver(1));
    Model model{m, std::vector<Invariant>{}};

    std::vector<SystemState> res = model.run();
    std::cout << "Simulation exited with " << res.size()
        << " terminating states" << std::endl;
    return 0;
}
