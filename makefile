CXXFLAGS := -Wall -std=c++20 $(CXXFLAGS)
PROGS = example ack paxos
OBJDIR ?= build
BUILDSTAMP = $(OBJDIR)/stamp

all: $(PROGS)

# Disable implicit rules
%.o: %.cpp
%: %.o
%: %.cpp

# Debugging
D ?= 0
ifeq ($(D),1)
CXXFLAGS += -g
endif

ifneq ($(B),)
CXXFLAGS += -DB=1
endif

# Dependencies
DEPSOPTS = -MMD
DEPS = $(wildcard $(OBJDIR)/*.d)
ifneq ($(DEPS),)
include $(DEPS)
endif

# Rebuild if options change
ifneq ($(strip $(OLDFLAGS)),$(strip $(CXXFLAGS)))
OLDFLAGS := $(shell mkdir -p $(OBJDIR); touch $(BUILDSTAMP); echo "OLDFLAGS := $(CXXFLAGS)" > $(OBJDIR)/_opts.d)
endif


%: $(OBJDIR)/%.o $(OBJDIR)/model.o
	g++ $(CXXFLAGS) $(LDLIBS) $^ -o $@


$(OBJDIR)/%.o: %.cpp $(BUILDSTAMP)
	g++ $(CXXFLAGS) $(DEPSOPTS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(PROGS)

$(BUILDSTAMP):
	@mkdir -p $(OBJDIR)
	@touch $(BUILDSTAMP)

.PHONY: all clean
.PRECIOUS: $(OBJDIR)/%.o
