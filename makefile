CXXFLAGS := -Wall -std=c++20 $(CXXFLAGS)
PROGS = example
OBJDIR ?= build
BUILDSTAMP = $(OBJDIR)/stamp

all: $(PROGS)

# Disable implicit rules
%.o: %.cc
%: %.o
%: %.cc
%: %.cpp


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


# # This is an example Makefile for a countwords program.  This
# # program uses both the scanner module and a counter module.
# # Typing 'make' or 'make count' will create the executable file.


# # define some Makefile variables for the compiler and compiler flags
# # to use Makefile variables later in the Makefile: $()

# #  -g    adds debugging information to the executable file
# #  -Wall turns on most, but not all, compiler warnings


# # # for C++ define  CC = g++
# # CC = clang
# # # CFLAGS  = -g -Wall
# # CXXFLAGS := -Wall -std=c++11 $(CXXFLAGS)

# # default: example

# # # To create the executable file count we need the object files
# # # countwords.o, counter.o, and scanner.o:
# # #
# # example:  model.o example.o
# # 	$(CC) $(CXXFLAGS) -o example example.o model.o

# # # To create the object file countwords.o, we need the source
# # # files countwords.c, scanner.h, and counter.h:
# # #
# # model.o:  model.cpp model.hh
# # 	$(CC) $(CXXFLAGS) -c model.cpp

# # # To create the object file counter.o, we need the source files
# # # counter.c and counter.h:
# # #
# # example.o:  example.cpp model.hh
# # 	$(CC) $(CXXFLAGS) -c example.cpp


# # # To create the object file scanner.o, we need the source files
# # # scanner.c and scanner.h:
# # #

# # clean:
# # 	$(RM) example *.o *~

# CXX      := clang
# CXXFLAGS := -Wall -std=c++11 $(CXXFLAGS)
# # LDFLAGS  := -L/usr/lib -lstdc++ -lm
# BUILD    := ./build
# OBJ_DIR  := $(BUILD)/objects
# APP_DIR  := $(BUILD)/apps
# TARGET   := example
# INCLUDE  := -Iinclude/
# SRC      := $(wildcard src/*.cpp)

# OBJECTS  := $(SRC:%.cpp=$(OBJ_DIR)/%.o)
# DEPENDENCIES \
#          := $(OBJECTS:.o=.d)

# all: build $(APP_DIR)/$(TARGET)

# $(OBJ_DIR)/%.o: %.cpp
# 	@mkdir -p $(@D)
# 	clang $(CXXFLAGS) $(LDLIBS) $(INCLUDE) -c $< -MMD -o $@

# $(APP_DIR)/$(TARGET): $(OBJECTS)
# 	@mkdir -p $(@D)
# 	clang $(CXXFLAGS) $(LDLIBS) -o $(APP_DIR)/$(TARGET) $^

# -include $(DEPENDENCIES)

# .PHONY: all build clean debug release info

# build:
# 	@mkdir -p $(APP_DIR)
# 	@mkdir -p $(OBJ_DIR)

# debug: CXXFLAGS += -DDEBUG -g
# debug: all

# release: CXXFLAGS += -O2
# release: all

# clean:
# 	-@rm -rvf $(OBJ_DIR)/*
# 	-@rm -rvf $(APP_DIR)/*

# info:
# 	@echo "[*] Application dir: ${APP_DIR}     "
# 	@echo "[*] Object dir:      ${OBJ_DIR}     "
# 	@echo "[*] Sources:         ${SRC}         "
# 	@echo "[*] Objects:         ${OBJECTS}     "
# 	@echo "[*] Dependencies:    ${DEPENDENCIES}"
