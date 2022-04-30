#include <stdint.h>
#include <string.h>
#include <list>
#include <vector>

#define iprintf(indent, fmt, ...) printf("%*s" fmt, indent, "", ##__VA_ARGS__)

// Timers are represented by messages with null data, src set to the timer id,
// and sz set to when the timer arrives - they can be distinguished by the null
// data
struct msg {
    uint64_t src;
    uint64_t dst;
    size_t sz;
    void* data;
    void print(int indent);
};

struct state;

struct machine {
    // subclasses should never modify two these directly
    uint64_t id;
    state* st;
    std::vector<msg> queue;
    void set_timer(uint64_t id, uint64_t timeout);
    void send_message(uint64_t dst, void* data, size_t sz);
    [[noreturn]] void fail(const char* msg);
    virtual void init() = 0;
    virtual void handle_timer(uint64_t id) = 0;
    virtual void handle_message(uint64_t src, void* data, size_t sz) = 0;
    virtual void print(int indent) = 0;
};

struct state {
    uint64_t time = 0;
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
bool predicate (state* s);
