#include "model.hpp"

// A simple example of many sender machines sending messages to a single
// receiver. Due to network asynchrony, the receiver may receive them in any
// order.

#define MCH_SND 1
#define MCH_RCV 2

struct Sender : Machine {
    id_t dst;

    Sender(id_t id, id_t dst) : Machine(id, MCH_SND), dst(dst) {}

    Sender* clone() const override {
        return new Sender(id, dst);
    }

    // The sender sends a message on startup, but does no message handling
    std::vector<Message*> on_startup() override {
        std::vector<Message*> ret;
        ret.push_back(new Message(id, dst, 0));
        printf("Sender %u sent its message.\n", id);
        return ret;
    }

    int sub_compare(Machine* rhs) const override {
        return 0;
    }
};

struct Receiver : Machine {
    // A receiver has an ordered log of the ids of machines from whom it has
    // received messages.
    std::vector<id_t> log;

    Receiver(id_t id) : Machine(id, MCH_RCV) {}

    Receiver* clone() const override {
        Receiver* r = new Receiver(id);
        r->log = log;
        return r;
    }

    // The receiver receives messages, but does nothing on startup
    std::vector<Message*> handle_message(Message* m) override {
        // log.push_back(m->src);
        log.push_back(0);
        return std::vector<Message*>{};
    }

    int sub_compare(Machine* rhs) const override {
        Receiver* m = dynamic_cast<Receiver*>(rhs);
        if (long r = log.size() - m->log.size()) return r;
        return memcmp(log.data(), m->log.data(), log.size() * sizeof(int));
    }
};


int main(int argc, char** argv) {
    size_t n = 9;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [n]\n    n: number of senders\n", argv[0]);
        return 1;
    }
    if (argc > 1 && *argv[1]) {
        char* end = nullptr;
        size_t senders = strtoul(argv[1], &end, 10);
        if (*end) {
            fprintf(stderr, "invalid number %s\n", argv[1]);
            fprintf(stderr, "usage: %s [n]\n    n: number of senders\n", argv[0]);
            return 1;
        }
        n = senders;
    }
    std::vector<Machine*> m;
    m.push_back(new Receiver(0));
    for (size_t i = 1; i <= n; i++) {
        m.push_back(new Sender(i, 0));
    }
    std::vector<Predicate> i;
    auto pred = [n] (const SystemState& s) {
        // All the sent messages plus everything in the log should cooperatively
        // contain all numbers only once
        // For some reason we have to cast void* to unsigned*
        unsigned* counts = (unsigned*) malloc(n * sizeof *counts);
        memset(counts, 0, n * sizeof *counts);
        for (size_t i = 0; i < s.messages.size(); ++i) {
            counts[s.messages[i]->src]++;
        }
        Receiver* r = dynamic_cast<Receiver*>(s.machines[0]);
        for (size_t i = 0; i < r->log.size(); ++i) {
            counts[r->log[i]]++;
        }
        for (size_t i = 1; i < n; ++i) {
            if (counts[i] != 1) {
                free(counts);
                return false;
            }
        }
        free(counts);
        return true;
    };
    // i.push_back(Invariant{"Basic", pred});
    Model model{m, i};

    std::set<SystemState> res = model.run();
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
