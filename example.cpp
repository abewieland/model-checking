#include "model.hpp"
#include "unistd.h"

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
    bool ordered;

    Receiver(id_t id, bool ordered) : Machine(id, MCH_RCV), ordered(ordered) {}

    Receiver* clone() const override {
        Receiver* r = new Receiver(id, ordered);
        r->log = log;
        return r;
    }

    // The receiver receives messages, but does nothing on startup
    std::vector<Message*> handle_message(Message* m) override {
        log.push_back(ordered ? m->src : 0);
        return std::vector<Message*>{};
    }

    int sub_compare(Machine* rhs) const override {
        Receiver* m = dynamic_cast<Receiver*>(rhs);
        if (long r = log.size() - m->log.size()) return r;
        return memcmp(log.data(), m->log.data(), log.size() * sizeof(int));
    }
};

void print_usage(const char* progname) {
    fprintf(stderr, "usage: %s [-h] [-n senders] [-o]\n"
                    "   -h: print this help message and exit\n"
                    "   -n: number of senders; defaults to 9\n"
                    "   -o: should ordering matter; defaults to no\n",
                    progname);
}

int main(int argc, char** argv) {
    // parse args
    size_t n = 9;
    bool ordered = false;
    int c;
    char* end;
    while ((c = getopt(argc, argv, "hn:o")) != -1) {
        switch(c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'n':
                end = nullptr;
                n = strtoul(optarg, &end, 10);
                if (*end) {
                    fprintf(stderr, "%s: invalid number of senders %s\n",
                            argv[0], optarg);
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 'o':
                ordered = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    if (optind != argc) {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        print_usage(argv[0]);
        return 1;
    }

    std::vector<Machine*> m;
    m.push_back(new Receiver(0, ordered));
    for (size_t i = 1; i <= n; i++) {
        m.push_back(new Sender(i, 0));
    }
    std::vector<Predicate> i;
    if (ordered) {
        auto pred = [n] (const SystemState& s) {
            // All the sent messages plus everything in the log should
            // cooperatively contain all numbers only once
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
        i.push_back(Predicate{"Basic", pred});
    }
    Model model{m, i};

    std::set<SystemState> res = model.run(-1, !ordered);
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
