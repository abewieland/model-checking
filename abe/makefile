CXXFLAGS := -Wall -std=c++11 $(CXXFLAGS)
PROGS = msg
OBJDIR ?= build
BUILDSTAMP = $(OBJDIR)/stamp

all: $(PROGS)

# Disable implicit rules
%.o: %.cc
%: %.o
%: %.cc

# Debugging
G ?= 0
ifeq ($(G),1)
CXXFLAGS += -g
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

$(OBJDIR)/%.o: %.cc $(BUILDSTAMP)
	g++ $(CXXFLAGS) $(DEPSOPTS) -c $< -o $@

%: $(OBJDIR)/%.o $(OBJDIR)/simple.o
	g++ $(CXXFLAGS) $(LDLIBS) $^ -o $@

clean:
	rm -rf $(OBJDIR) $(PROGS)

$(BUILDSTAMP):
	@mkdir -p $(OBJDIR)
	@touch $(BUILDSTAMP)

.PHONY: all clean
.PRECIOUS: $(OBJDIR)/%.o
