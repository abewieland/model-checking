#include <stdlib.h>
#include <time.h>
#include "model.hpp"

struct Timer : Message {
    using Message::Message;
};

struct Ack : Message {
    using Message::Message;
};

struct Msg : Message {
    int val;

    Msg(int src, int dst, int val) : Message(src, dst) {
        this->val = val;
    }
};

struct Sender : Machine {
    bool ack;
    int dst;
    int val;

    Sender(int id, int dst, int val) {
        this->id = id;
        this->dst = dst;
        this->val = val;
        this->ack = false;
    }

    Sender* clone() const override {
        Sender* s = new Sender(id, dst, val);
        s->ack = ack;
        return s;
    }

    std::vector<Message*> handle_message(Message& m) override {
        // Because of cyclic reference issues (I can't use dynamic_cast in
        // Message::operate unless it's a full type, nor can I do typeid here
        // unless it's a full type), I decided to just put all the functionality
        // here
        std::vector<Message*> ret;
        // The type checking stuff turns out to be fairly easy to write, though
        // I hate to think of what is occurring behind the scenes
        if (typeid(m) == typeid(Timer)) {
            if (!ack) {
                ret.push_back(new Msg(id, dst, val));
                ret.push_back(new Timer(id, id));
            }
        } else if (typeid(m) == typeid(Ack)) {
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
    bool recv;
    int val;

    Receiver(int id) {
        this->id = id;
        this->val = -1;
    }

    Receiver* clone() const override {
        Receiver* r = new Receiver(id);
        r->val = val;
        r->recv = recv;
        return r;
    }

    std::vector<Message*> handle_message(Message& m) override {
        // Pretty simple for the receiver - send acknowledgement
        std::vector<Message*> ret;
        val = dynamic_cast<Msg&>(m).val;
        ret.push_back(new Ack(id, m.src));
        return ret;
    }
};


int main() {
    srand(time(0));
    std::vector<Machine*> m;
    m.push_back(new Sender(0, 1, rand()));
    m.push_back(new Receiver(1));
    SystemState initial(m);
    Model model(initial, std::vector<Invariant>());

    std::vector<SystemState> res = model.run();
    std::cout << "Simulation exited with " << res.size()
        << " terminating states" << std::endl;
}
