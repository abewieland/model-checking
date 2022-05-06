#include <time.h>
#include "model.hpp"

// An example of n-way replication, replicated from that in the P# paper

#define MSG_TIME  1
#define MSG_CLNT  2
#define MSG_REPL  3
#define MSG_SYNC  4
#define MSG_ACK   5

typedef unsigned long data_t;

// A message with a simple data payload, used both between the client and the
// server and between the server and nodes
struct Payload : Message {
    data_t data;
    Payload(id_t src, id_t dst, int type, data_t data)
        : Message(src, dst, type), data(data) {}

    int sub_compare(Message* rhs) const override {
        return data - dynamic_cast<Payload*>(rhs)->data;
    }
};

struct Client : Machine {
    id_t server;
    std::vector<data_t> data;
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
            ++index;
        }
        return ret;
    }
};

struct Server : Machine {
    id_t client;
    id_t first_node;
    size_t nodes;
    unsigned index;
    #if B > 0
    unsigned repcount;
    #else
    std::vector<bool> reps;
    #endif

    Server(id_t id, id_t client, id_t first_node, size_t nodes)
        : Machine(id), client(client), first_node(first_node), nodes(nodes),
          index(0) {
        #if B > 0
        repcount = 0;
        #else
        reps.reserve(nodes);
        #endif
    }

    Server* clone() const override {
        Server* s = new Server(id, client, first_node, nodes);
        s->index = index;
        #if B > 0
        s->repcount = repcount;
        #else
        s->reps = reps;
        #endif
        return s;
    }

    int compare(Machine* rhs) const override {
        if (int r = (int) id - rhs->id) return r;
        #if B > 0
        if (int r = (int) repcount - rhs->repcount) return r;
        #else
        // This is dumb, but whatever (they may be bit vectors, so we can't use
        // memcmp, and <=> gives a non-integer result)
        if (reps < rhs->reps) return -1;
        if (reps > rhs->reps) return 1;
        #endif
        return index - dynamic_cast<Server*>(rhs)->index;
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        switch (m->type) {
            case MSG_CLNT:
                
                break;
            case MSG_SYNC:
                
                break;
            // do nothing in other cases
        }
        return ret;
    }
};

struct Node : Machine {
    // Stuff here!
};

int main() {
    // Argument parsing!!
    srand(time(0));
    std::vector<Machine*> m;
    std::vector<Invariant> i;
    Model model{m, i};

    std::vector<SystemState> res = model.run();
    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
