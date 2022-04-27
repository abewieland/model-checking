#include <stdint.h>
#include <list>
#include <vector>

// Timers are represented by messages with null data, zero sz, src set to the
// current machine, and dst set to the timer id - they are thus the only msgs
// with null data, zero sz, and dst potentially not matching the machine's id
struct msg {
    uint64_t src;
    uint64_t dst;
    size_t sz;
    void* data;
};

struct machine {
    // subclasses should never modify two these directly
    uint64_t id;
    std::vector<msg> queue;
    void set_timer(uint64_t id, uint64_t timeout);
    void send_message(uint64_t dst, size_t sz, void* data);
    virtual void init() = 0;
    virtual void handle_timer(uint64_t id) = 0;
    virtual void handle_message(uint64_t src, size_t sz, void* data) = 0;
    virtual bool predicate() = 0;
    virtual void print(int indent) = 0;
};

struct state {
    std::vector<machine*> m;
    void print(int indent);
};

struct history {
    std::list<state*> prev;
    state* curr;
    void print(int indent);
};

// The actual machine concrete implementation is written by the test, as are
// the following functions:
state* init_state();
