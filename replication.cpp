#include <random>
#include "model.hpp"
#include "unistd.h"

// An implementation of n-way replication, inspired by the P# paper

#define MSG_TIME  1
#define MSG_CLNT  2
#define MSG_REPL  3
#define MSG_SYNC  4
#define MSG_ACK   5

#define MCH_CLNT  1
#define MCH_SRV   2
#define MCH_NODE  3

typedef unsigned long data_t;

// A message with a simple data payload, used for both CLNT and REPL messages
struct Payload : Message {
    data_t data;
    Payload(id_t src, id_t dst, int type, data_t data)
        : Message(src, dst, type), data(data) {}

    int sub_compare(Message* rhs) const override {
        return data - dynamic_cast<Payload*>(rhs)->data;
    }

    void sub_print() const override {
        printf("    Data: %lu\n", data);
    }
};

struct Sync : Message {
    int index;
    Sync(id_t src, id_t dst, int index)
        : Message(src, dst, MSG_SYNC), index(index) {}

    int sub_compare(Message* rhs) const override {
        return index - dynamic_cast<Sync*>(rhs)->index;
    }

    void sub_print() const override {
        printf("    Index: %d\n", index);
    }
};

struct Client : Machine {
    id_t server;
    std::vector<data_t> data;
    // The next data to send (one past the last acknowledged)
    unsigned index;

    Client(id_t id, id_t server, std::vector<data_t> data)
        : Machine(id, MCH_CLNT), server(server), data(data), index(0) {}

    Client* clone() const override {
        Client* c = new Client(id, server, data);
        c->index = index;
        return c;
    }

    int sub_compare(Machine* rhs) const override {
        return index - dynamic_cast<Client*>(rhs)->index;
    }

    std::vector<Message*> on_startup() override {
        std::vector<Message*> ret;
        ret.push_back(new Payload(id, server, MSG_CLNT, data[0]));
        return ret;
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        if (m->type == MSG_ACK) {
            if (++index < data.size()) {
                ret.push_back(new Payload(id, server, MSG_CLNT, data[index]));
            }
        }
        return ret;
    }
};

struct Server : Machine {
    id_t client;
    id_t first_node;
    size_t nodes;
    int index;
    data_t data;
    #ifdef B
    unsigned repcount;
    #else
    std::vector<bool> reps;
    #endif

    Server(id_t id, id_t client, id_t first_node, size_t nodes)
        : Machine(id, MCH_SRV), client(client), first_node(first_node),
          nodes(nodes), index(-1) {
        #ifdef B
        repcount = 0;
        #else
        reps.assign(nodes, false);
        #endif
    }

    Server* clone() const override {
        Server* s = new Server(id, client, first_node, nodes);
        s->index = index;
        s->data = data;
        #ifdef B
        s->repcount = repcount;
        #else
        s->reps = reps;
        #endif
        return s;
    }

    int sub_compare(Machine* rhs) const override {
        Server* s = dynamic_cast<Server*>(rhs);
        if (int r = index - s->index) return r;
        if (long r = (long) data - s->data) return r;
        #ifdef B
        if (int r = (int) repcount - s->repcount) return r;
        #else
        // This is dumb, but whatever (they may be bit vectors, so we can't use
        // memcmp, and <=> gives a non-integer result)
        if (reps < s->reps) return -1;
        if (reps > s->reps) return 1;
        #endif
        return 0;
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        switch (m->type) {
            case MSG_CLNT:
                #ifdef B
                repcount = 0;
                #else
                reps.assign(nodes, false);
                #endif
                ++index;
                data = dynamic_cast<Payload*>(m)->data;
                for (size_t i = 0; i < nodes; ++i) {
                    ret.push_back(new Payload(id, first_node + i, MSG_REPL, data));
                }
                break;
            case MSG_SYNC:
                int ind = dynamic_cast<Sync*>(m)->index;
                if (ind < index) {
                    ret.push_back(new Payload(id, m->src, MSG_REPL, data));
                } else {
                    #ifdef B
                    if (++repcount == nodes) {
                        ret.push_back(new Message(id, client, MSG_ACK));
                    }
                    #else
                    reps[m->src] = true;
                    size_t i;
                    for (i = 0; i < nodes; ++i) {
                        if (!reps[i]) break;
                    }
                    if (i == nodes) {
                        ret.push_back(new Message(id, client, MSG_ACK));
                    }
                    #endif
                }
                break;
            // do nothing in other cases
        }
        return ret;
    }
};

struct Node : Machine {
    id_t server;
    bool timer;
    std::vector<data_t> log;

    Node(id_t id, id_t server)
        : Machine(id, MCH_NODE), server(server), timer(false) {}

    Node* clone() const override {
        Node* n = new Node(id, server);
        n->timer = timer;
        n->log = log;
        return n;
    }

    int sub_compare(Machine* rhs) const override {
        Node* n = dynamic_cast<Node*>(rhs);
        if (int r = (int) timer - n->timer) return r;
        if (long r = (long) log.size() - n->log.size()) return r;
        return memcmp(log.data(), n->log.data(), log.size() * sizeof(data_t));
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        switch (m->type) {
            case MSG_REPL:
                log.push_back(dynamic_cast<Payload*>(m)->data);
                if (!timer) {
                    timer = true;
                    ret.push_back(new Message(id, id, MSG_TIME));
                }
                break;
            case MSG_TIME:
                ret.push_back(new Message(id, id, MSG_TIME));
                ret.push_back(new Sync(id, server, log.size()));
                break;
        }
        return ret;
    }
};

void print_usage(const char* progname) {
    fprintf(stderr, "usage: %s [-h] [-n nodes] [-r rounds]\n"
                    "   -h: print this help message and exit\n"
                    "   -n: number of replication nodes; defaults to 3\n"
                    "   -r: number of data items to send; defaults to 1\n",
                    progname);
}

int main(int argc, char** argv) {
    // Parse args
    size_t nodes = 3;
    size_t rounds = 1;
    int c;
    char* end;
    while ((c = getopt(argc, argv, "hn:r:")) != -1) {
        switch(c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'n':
                end = nullptr;
                nodes = strtoul(optarg, &end, 10);
                if (*end) {
                    fprintf(stderr, "%s: invalid number of nodes %s\n",
                            argv[0], optarg);
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 'r':
                end = nullptr;
                rounds = strtoul(optarg, &end, 10);
                if (*end) {
                    fprintf(stderr, "%s: invalid number of data items %s\n",
                            argv[0], optarg);
                    print_usage(argv[0]);
                    return 1;
                }
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

    std::vector<data_t> data;
    std::mt19937_64 r;
    for (size_t i = 0; i < rounds; ++i) {
        data.push_back(r());
    }
    std::vector<Machine*> m;
    m.push_back(new Client(0, 1, data));
    m.push_back(new Server(1, 0, 2, nodes));
    for (size_t i = 2; i < 2 + nodes; ++i) {
        m.push_back(new Node(i, 1));
    }
    std::vector<Predicate> i;
    auto pred = [nodes, rounds] (const SystemState& s) {
        Client* c = dynamic_cast<Client*>(s.machines[0]);
        if (!c->index) return true;
        unsigned ind = c->index - 1;
        for (size_t i = 2; i < 2 + nodes; ++i) {
            Node* n = dynamic_cast<Node*>(s.machines[i]);
            if (n->log.size() <= ind) return false;
            if (n->log[ind] != c->data[ind]) return false;
        }
        return true;
    };
    i.push_back(Predicate{"Ack not received before replicated", pred});
    Model model{m, i};

    std::set<SystemState> res = model.run();
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
