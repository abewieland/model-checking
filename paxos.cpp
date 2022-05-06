#include <time.h>
#include <limits.h>
#include "model.hpp"


#define MSG_PREPARE 1
#define MSG_PREPARE_OK 2
#define MSG_ACCEPT 3
#define MSG_ACCEPT_OK 4
#define MSG_SEND_PROPOSAL 5


// #define MSG_ACK   2
// #define MSG_VAL   3

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

    PrepareOk(id_t src, id_t dst, int n, int na, int va) : Message(src, dst, MSG_PREPARE_OK), n(n), na(na), va(va) {}

    int sub_compare(Message* rhs) const override {
        if (int r = n - dynamic_cast<PrepareOk*>(rhs)->n) return r;
        if (int r = na - dynamic_cast<PrepareOk*>(rhs)->na) return r;
        return va - dynamic_cast<PrepareOk*>(rhs)->va;
    }
};


struct Accept : Message {
    int n;
    int v;

    Accept(id_t src, id_t dst, int n, int v) : Message(src, dst, MSG_ACCEPT), n(n), v(v) {}

    int sub_compare(Message* rhs) const override {
        if (int r = n - dynamic_cast<Accept*>(rhs)->n) return r;
        return v - dynamic_cast<Accept*>(rhs)->v;
    }
};

struct AcceptOk : Message {
    int n;
    AcceptOk(id_t src, id_t dst, int n) : Message(src, dst, MSG_ACCEPT_OK), n(n) {}

    int sub_compare(Message* rhs) const override {
        return n - dynamic_cast<AcceptOk*>(rhs)->n;
    }
};

struct SendProposal : Message {
    int v;
    SendProposal(id_t src, id_t dst, int v) : Message(src, dst, MSG_SEND_PROPOSAL), v(v) {}

    int sub_compare(Message* rhs) const override {
        return v - dynamic_cast<SendProposal*>(rhs)->v;
    }
};

struct StateMachine : Machine {
    int cluster_size;
    int np;
    int na;
    int va;
    bool should_propose;

    std::vector<PrepareOk*> prepares_received;
    std::vector<AcceptOk*> accepts_received;

    int selected_n = -1;
    int selected_v_prime = -1;
    int final_value = -1;

    StateMachine(id_t id, int cluster_size, int np, int na, int va, bool should_propose)
        : Machine(id), cluster_size(cluster_size), np(np), na(na), va(va), should_propose(should_propose) {}

    StateMachine(id_t id, int cluster_size, bool should_propose)
        : Machine(id), cluster_size(cluster_size), np(-1), na(-1), va(-1), should_propose(should_propose) {}

    StateMachine* clone() const override {
        return new StateMachine(id, cluster_size, np, na, va, should_propose);
    }

    int count_prepares(int target_n) {
        int result = 0;
        for(PrepareOk*& p : prepares_received) {
            if(p->n == target_n) {
                result++;
            }
        }
        return result;
    }

    int count_accepts(int target_n) {
        int result = 0;
        for(AcceptOk*& a : accepts_received) {
            if(a->n == target_n) {
                result++;
            }
        }
        return result;
    }

    int v_from_max_na(int target_n) {
        int highest_na = INT_MIN;
        int ret = INT_MIN;
        for(PrepareOk*& p : prepares_received) {
            if(p->n == target_n) {
                if(p->na > highest_na) {
                    highest_na = p->na;
                    ret = p->va;
                }
            }
        }
        return ret;
    }

    std::vector<Message*> handle_message(Message* m) override {
        std::vector<Message*> ret;
        switch (m->type) {
            case MSG_SEND_PROPOSAL:
            {
                int n = np + 1;
                selected_n = n;
                for(id_t i = 0; i < cluster_size; ++i) {
                    ret.push_back(new Prepare(this->id, i, n));
                }
            }
            break;
            case MSG_PREPARE:
            {
                int n = dynamic_cast<Prepare*>(m)->n;
                if(n > np) {
                    this->np = n;
                    ret.push_back(new PrepareOk(this->id, m->src, n, na, va));
                }
            }
            break;
            case MSG_PREPARE_OK:
            {
                PrepareOk* m = dynamic_cast<PrepareOk*>(m);
                prepares_received.push_back(m);
                int prepares_received = count_prepares(selected_n);
                if(prepares_received > cluster_size / 2) {
                    int v_prime = v_from_max_na(selected_n);
                    for(id_t i = 0; i < cluster_size; ++i) {
                        selected_v_prime = v_prime;
                        ret.push_back(new Accept(this->id, i, selected_n, v_prime));
                    }
                }
            }
            break;
            case MSG_ACCEPT:
            {
                Accept* m = dynamic_cast<Accept*>(m);
                int n = m->n;
                int v = m->v;
                if(n > np) {
                    this->np = n;
                    this->na = n;
                    this->va = v;
                    ret.push_back(new AcceptOk(this->id, m->src, n));
                }
            }
            break;
            case MSG_ACCEPT_OK:
            {
                AcceptOk* m = dynamic_cast<AcceptOk*>(m);
                accepts_received.push_back(m);
                int accepts_received = count_accepts(selected_n);
                if(accepts_received > (cluster_size / 2)) {
                    final_value = selected_v_prime;
                }
            }
            break;
            default:
            break;
        }
        return ret;
    }

    std::vector<Message*> on_startup() override {
        std::vector<Message*> ret;
        if(should_propose) {
            ret.push_back(new SendProposal(id, id, id + 100));
        }
        return ret;
    }

    int compare(Machine* rhs) const override {
        // pain
        if (int r = (int) id - rhs->id) return r;
        StateMachine* m = dynamic_cast<StateMachine*>(rhs);
        if (int r = np - m->np) return r;
        if (int r = na - m->na) return r;
        if (int r = va - m->va) return r;
        if (int r = selected_n - m->selected_n) return r;
        if (int r = selected_v_prime - m->selected_v_prime) return r;
        if (int r = final_value - m->final_value) return r;
        if (long r = prepares_received.size() - m->prepares_received.size()) return r;
        if (long r = accepts_received.size() - m->accepts_received.size()) return r;
        if (long r = (memcmp(prepares_received.data(), m->prepares_received.data(), prepares_received.size() * sizeof(int)))) return r;
        return memcmp(accepts_received.data(), m->accepts_received.data(), accepts_received.size() * sizeof(int));
    }
};


// bool invariant(SystemState st) {
//     Sender* s = dynamic_cast<Sender*>(st.machines[0]);
//     Receiver* r = dynamic_cast<Receiver*>(st.machines[1]);
//     if (r->recv && r->val != s->val) return false;
//     if (s->ack && r->val != s->val) return false;
//     return true;
// }

int main() {
    int num_machines = 3;
    int proposer = 0;
    std::vector<Machine*> m;

    for(id_t i = 0; i < num_machines; i++) {
        m.push_back(new StateMachine(i, num_machines, (i==proposer)));
    }

    std::vector<Invariant> i;
    // i.push_back(Invariant{"Consistency", invariant});

    Model model{m, i};
    printf("Constructed\n");

    std::vector<SystemState> res = model.run();

    printf("Simluation exited with %lu terminating states.\n", res.size());
    return 0;
}
