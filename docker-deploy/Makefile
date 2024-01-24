# Define variables
CXX = g++
CXXFLAGS = -std=c++17 -pedantic -Wall -Wextra -Werror -ggdb3 -pthread
INCDIR = include
SRCDIR = src
BINDIR = bin
LOG = log
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(BINDIR)/%.o, $(SOURCES))
EXECUTABLE = $(BINDIR)/http_cache_proxy

# Default target: build and run the program
all: clean build run

# Build the program
build: $(EXECUTABLE)

# Run the program
run: $(EXECUTABLE)
	./$(EXECUTABLE)

# Clean the build
clean:
	rm -rf $(BINDIR)
	rm -rf $(LOG)
	mkdir -p $(BINDIR)

# Create the bin directory if it does not exist
$(BINDIR):
	mkdir -p $(BINDIR)

# Build the executable
$(EXECUTABLE): $(OBJECTS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -luuid -o $@

# Compile the source files
$(BINDIR)/%.o: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

# Manage dependencies automatically
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(BINDIR)/%.d: $(SRCDIR)/%.cpp | $(BINDIR)
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) -I$(INCDIR) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(BINDIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: all build run clean
