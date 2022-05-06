#include "model.hpp"

// An implementation of n-way replication, inspired by the P# paper

#define MSG_TIME  1
#define MSG_CLNT  2
#define MSG_REPL  3
#define MSG_SYNC  4
#define MSG_ACK   5

typedef unsigned long data_t;

// A message with a simple data payload, used for both CLNT and REPL messages
struct Payload : Message {
    data_t data;
    Payload(id_t src, id_t dst, int type, data_t data)
        : Message(src, dst, type), data(data) {}

    int sub_compare(Message* rhs) const override {
        return data - dynamic_cast<Payload*>(rhs)->data;
    }
};

struct Sync : Message {
    int index;
    Sync(id_t src, id_t dst, int index)
        : Message(src, dst, MSG_SYNC), index(index) {}

    int sub_compare(Message* rhs) const override {
        return index - dynamic_cast<Sync*>(rhs)->index;
    }
};

struct Client : Machine {
    id_t server;
    std::vector<data_t> data;
    // The next data to send (one past the last acknowledged)
    unsigned index;

    Client(id_t id, id_t server, std::vector<data_t> data)
        : Machine(id), server(server), data(data), index(0) {}

    Client* clone() const override {
        Client* c = new Client(id, server, data);
        c->index = index;
        return c;
    }

    int compare(Machine* rhs) const override {
        if (int r = (int) id - rhs->id) return r;
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
    #if B & 0x1
    unsigned repcount;
    #else
    std::vector<bool> reps;
    #endif

    Server(id_t id, id_t client, id_t first_node, size_t nodes)
        : Machine(id), client(client), first_node(first_node), nodes(nodes),
          index(-1) {
        #if B & 0x1
        repcount = 0;
        #else
        reps.assign(nodes, false);
        #endif
    }

    Server* clone() const override {
        Server* s = new Server(id, client, first_node, nodes);
        s->index = index;
        s->data = data;
        #if B & 0x1
        s->repcount = repcount;
        #else
        s->reps = reps;
        #endif
        return s;
    }

    int compare(Machine* rhs) const override {
        if (int r = (int) id - rhs->id) return r;
        Server* s = dynamic_cast<Server*>(rhs);
        if (int r = index - s->index) return r;
        if (long r = (long) data - s->data) return r;
        #if B & 0x1
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
                #if !(B & 0x2)
                #if B & 0x1
                repcount = 0;
                #else
                reps.assign(nodes, false);
                #endif
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
                    #if B & 0x1
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

    Node(id_t id, id_t server) : Machine(id), server(server), timer(false) {}

    Node* clone() const override {
        Node* n = new Node(id, server);
        n->timer = timer;
        n->log = log;
        return n;
    }

    int compare(Machine* rhs) const override {
        if (int r = (int) id - rhs->id) return r;
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

int main() {
    // Argument parsing - maybe later!!
    size_t nodes = 3;
    size_t rounds = 1;
    std::vector<data_t> data;
    data.reserve(rounds);
    FILE* f = fopen("/dev/random", "r");
    if (!f) {
        perror("open");
        return 1;
    }
    fread(data.data(), 1, rounds * sizeof(data_t), f);
    fclose(f);
    std::vector<Machine*> m;
    m.push_back(new Client(0, 1, data));
    m.push_back(new Server(1, 0, 2, nodes));
    for (size_t i = 2; i < 2 + nodes; ++i) {
        m.push_back(new Node(i, 1));
    }
    std::vector<Invariant> i;
    Model model{m, i};

    std::vector<SystemState> res = model.run();
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
