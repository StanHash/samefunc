# project config

TARGET := samefunc
SOURCES := main.cc elf32.cc func.cc
BUILDDIR := build

CXX ?= g++
CXXFLAGS := -Wall -Wextra -std=c++20 -g -Og

# work

OBJECTS := $(SOURCES:%.cc=$(BUILDDIR)/%.o)
DEPENDS := $(SOURCES:%.cc=$(BUILDDIR)/%.d)

all: $(TARGET)

clean:
	rm -rf $(BUILDDIR)
	rm -f $(TARGET)

$(BUILDDIR)/%.d: %.cc
	@mkdir -p $(dir $@)
	@$(CXX) $< $(CXXFLAGS) -o $@ -MM -MG -MT $@ -MT $(BUILDDIR)/$*.o

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(CXXFLAGS) -o $(TARGET)

$(BUILDDIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $< $(CXXFLAGS) -c -o $@

-include $(DEPENDS)
