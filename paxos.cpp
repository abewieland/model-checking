#include <time.h>
#include <iostream>
#include <limits.h>
#include "model.hpp"

#define MSG_PREPARE        1
#define MSG_PREPARE_OK     2
#define MSG_ACCEPT         3
#define MSG_ACCEPT_OK      4
#define MSG_SEND_PROPOSAL  5

// Variable names all match
// http://css.csail.mit.edu/6.824/2014/notes/paxos-code.html

struct Prepare : Message {
    int n;
    Prepare(id_t src, id_t dst, int n) : Message(src, dst, MSG_PREPARE), n(n) {}

    int sub_compare(Message* rhs) const override {
        return n - dynamic_cast<Prepare*>(rhs)->n;
    }
};

struct PrepareOk : Message {
    int n;
    int na;
    int va;

    PrepareOk(id_t src, id_t dst, int n, int na, int va)
        : Message(src, dst, MSG_PREPARE_OK), n(n), na(na), va(va) {}

    int sub_compare(Message* rhs) const override {
        if (int r = n - dynamic_cast<PrepareOk*>(rhs)->n) return r;
        if (int r = na - dynamic_cast<PrepareOk*>(rhs)->na) return r;
        return va - dynamic_cast<PrepareOk*>(rhs)->va;
    }
};

struct Accept : Message {
    int n;
    int v;

    Accept(id_t src, id_t dst, int n, int v)
        : Message(src, dst, MSG_ACCEPT), n(n), v(v) {}

    int sub_compare(Message* rhs) const override {
        if (int r = n - dynamic_cast<Accept*>(rhs)->n) return r;
        return v - dynamic_cast<Accept*>(rhs)->v;
    }
};

struct AcceptOk : Message {
    int n;
    AcceptOk(id_t src, id_t dst, int n)
        : Message(src, dst, MSG_ACCEPT_OK), n(n) {}

    int sub_compare(Message* rhs) const override {
        return n - dynamic_cast<AcceptOk*>(rhs)->n;
    }
};

struct SendProposal : Message {
    int v;
    SendProposal(id_t src, id_t dst, int v)
        : Message(src, dst, MSG_SEND_PROPOSAL), v(v) {}

    int sub_compare(Message* rhs) const override {
        return v - dynamic_cast<SendProposal*>(rhs)->v;
    }
};

struct StateMachine : Machine {
    int cluster_size;

    int np;
    int na;
    int va;

    // On startup, this machine will propose a value
    // by sending a message to itself, requesting a proposal.
    bool should_propose;

    std::set<PrepareOk*> prepares_received;
    std::set<AcceptOk*> accepts_received;

    // Right now our paxos can only select postive values and
    // I'm okay with that.
    int selected_n = -1;
    int selected_v_prime = -1;
    int final_value = -1;

    StateMachine(id_t id, int sz, int np, int na, int va, bool propose)
        : Machine(id, 0), cluster_size(sz), np(np), na(na), va(va),
          should_propose(propose) {}

    StateMachine(id_t id, int sz, int np, int na, int va, bool propose,
                 std::set<PrepareOk*> prepares_received,
                 std::set<AcceptOk*> accepts_received,
                 int selected_n, int selected_v_prime, int final_value)
        : StateMachine(id, sz, np, na, va, propose) {
            this->prepares_received = prepares_received;
            this->accepts_received = accepts_received;
            this->selected_n = selected_n;
            this->selected_v_prime = selected_v_prime;
            this->final_value = final_value;
        }

    StateMachine(id_t id, int sz, bool propose)
        : StateMachine(id, sz, -1, -1, -1, propose) {}

    StateMachine* clone() const override {
        return new StateMachine(id, cluster_size, np, na, va, should_propose,
                                prepares_received, accepts_received, selected_n,
                                selected_v_prime, final_value);
    }

    int count_prepares(int target_n) {
        int result = 0;
        for(auto& p : prepares_received) {
            if(p->n == target_n) {
                result++;
            }
        }
        return result;
    }

    int count_accepts(int target_n) {
        int result = 0;
        for(auto& a : accepts_received) {
            if(a->n == target_n) {
                result++;
            }
        }
        return result;
    }

    int v_from_max_na(int target_n, int my_n, int my_v) {
        int highest_na = my_n;
        int ret = my_v;
        for(auto& p : prepares_received) {
            if(p->n == target_n) {
                if(p->na > highest_na) {
                    highest_na = p->na;
                    ret = p->va;
                }
            }
        }
        return ret;
    }


    std::vector<Message*> handle_proposal_request(SendProposal *m) {
        std::vector<Message*> ret;
        int n = id*np + 10;
        this->va=m->v;
        selected_n = n;
        for(int i = 0; i < cluster_size; ++i) {
            ret.push_back(new Prepare(this->id, i, n));
        }
        return ret;
    }

    std::vector<Message*> handle_prepare(Prepare *m) {
        std::vector<Message*> ret;
        int message_n = m->n;
        if(message_n > np) {
            this->np = message_n;
            ret.push_back(new PrepareOk(this->id, m->src, message_n, na, va));
        }
        return ret;
    }

    std::vector<Message*> handle_prepare_ok(PrepareOk *m) {
        std::vector<Message*> ret;
        prepares_received.insert(m);
        // printf("num prepareoks inserted %lu\n", prepares_received.size());
        int pr = count_prepares(selected_n);
        if(pr > (cluster_size / 2)) {
            int v_prime = v_from_max_na(selected_n, this->selected_n, va);
            selected_v_prime = v_prime;
            for(int i = 0; i < cluster_size; ++i) {
                ret.push_back(new Accept(this->id, i, selected_n, v_prime));
            }
        }
        return ret;
    }

    std::vector<Message*> handle_accept(Accept *m) {
        std::vector<Message*> ret;
        int n = m->n;
        int v = m->v;
        if(n >= np) {
            this->np = n;
            this->na = n;
            this->va = v;
            ret.push_back(new AcceptOk(this->id, m->src, n));
        }
        return ret;
    }

    std::vector<Message*> handle_accept_ok(AcceptOk *m) {
        // printf("accepting ok");
        std::vector<Message*> ret;
        accepts_received.insert(m);
        int accepts_received2 = count_accepts(selected_n);

        if(accepts_received2 > (cluster_size / 2)) {
            final_value = selected_v_prime;
            // printf("One path terminated\n");
        }
        return ret;
    }

    std::vector<Message*> handle_message(Message* m) override {
        switch (m->type) {
        case MSG_SEND_PROPOSAL:
            return handle_proposal_request(dynamic_cast<SendProposal*>(m));
        case MSG_PREPARE:
            return handle_prepare(dynamic_cast<Prepare*>(m));
        case MSG_PREPARE_OK:
            return handle_prepare_ok(dynamic_cast<PrepareOk*>(m));
        case MSG_ACCEPT:
            return handle_accept(dynamic_cast<Accept*>(m));
        case MSG_ACCEPT_OK:
            return handle_accept_ok(dynamic_cast<AcceptOk*>(m));
        }
        // Aborting after unknown message seems harsh; maybe we should exit more
        // cleanly in that case (have a dummy predicate that some value should
        // always be zero and set it to 1 if an unknown message arrives)
        //std::cerr << "Unhandled message type. Aborting." << std::endl;
        //std::abort();
    }

    std::vector<Message*> on_startup() override {
        std::vector<Message*> ret;
        if(should_propose) {
            ret.push_back(new SendProposal(id, id, id + 200));
        }
        return ret;
    }

    int sub_compare(Machine* rhs) const override {
        // pain
        StateMachine* m = dynamic_cast<StateMachine*>(rhs);
        if (int r = np - m->np) return r;
        if (int r = na - m->na) return r;
        if (int r = va - m->va) return r;
        if (int r = selected_n - m->selected_n) return r;
        if (int r = selected_v_prime - m->selected_v_prime) return r;
        if (int r = final_value - m->final_value) return r;
        if (long r = prepares_received.size() - m->prepares_received.size()) return r;
        if (long r = accepts_received.size() - m->accepts_received.size()) return r;
        for(auto it1 = prepares_received.begin(), it2 = m->prepares_received.begin(); it1 != prepares_received.end() && it2 != m->prepares_received.end(); ++it1, ++it2) {
            if (long r = (*it1)->compare((dynamic_cast<Message*>(*it2)))) return r;
        }
        for(auto it1 = accepts_received.begin(), it2 = m->accepts_received.begin(); it1 != accepts_received.end() && it2 != m->accepts_received.end(); ++it1, ++it2) {
            if (long r = (*it1)->compare((dynamic_cast<Message*>(*it2)))) return r;
        }
        return 0;
    }
};

int main() {
    int num_machines = 3;
    int proposer = 0;
    int proposer2 = 1;

    std::vector<Machine*> m;

    for(int i = 0; i < num_machines; i++) {
        m.push_back(new StateMachine(i, num_machines, proposer == i || proposer2 == i));
    }

    Model model{m};
    printf("Constructed\n");

    std::set<SystemState> res = model.run(19, true);

    for(SystemState i : res) {
        // for(Machine * m : i.machines) {
            StateMachine* sm = dynamic_cast<StateMachine*>(i.machines[0]);
            printf("Learned value of %d\n", sm->final_value);
        // }
    }

    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
