## -------------------------------------------------------------------------------------------------
## Define constants
all: alex

TARGET_DIR := bin

CF := -g0 -O3 -std=c++14 -Wextra -Wall -I.
LF := -g0 -O3 -std=c++14 -libverbs -lpthread -lzmq

CCCACHE_USE?=
CXX?= g++
CXX:= $(CCCACHE_USE) $(CXX)

BUILD_DIR = @mkdir -p $(dir $@)

## -------------------------------------------------------------------------------------------------
## Track dependencies
-include $(TARGET_DIR)/*.P
-include $(TARGET_DIR)/*/*.P
-include $(TARGET_DIR)/*/*/*.P
-include $(TARGET_DIR)/*/*/*/*.P
## -------------------------------------------------------------------------------------------------
## Grep all files relevant for the build (= all cpp files)
src_cpp := $(shell find rdma dht util -name "*.cpp")
src_obj := $(addprefix $(TARGET_DIR)/, $(patsubst %.cpp,%.o,$(src_cpp)))
## -------------------------------------------------------------------------------------------------
## Build the test program
tester_obj := $(src_obj) bin/Tester.o
tester: $(tester_obj)
	@if [ $(VERBOSE) ]; then echo $(CXX) -o tester $(tester_obj) $(LF); else echo $(CXX) -o tester; fi
	@$(CXX) -o tester $(tester_obj) $(LF)
## -------------------------------------------------------------------------------------------------
## Build the coordinator for exchanging rdma keys
coordinator_obj := $(src_obj) bin/Coordinator.o
coordinator: $(coordinator_obj)
	@if [ $(VERBOSE) ]; then echo $(CXX) -o coordinator $(coordinator_obj) $(LF); else echo $(CXX) -o coordinator; fi
	@$(CXX) -o coordinator $(coordinator_obj) $(LF)
## -------------------------------------------------------------------------------------------------
## Build the performance measurement tool
perf_obj := $(src_obj) bin/src/Perf.o
perf: $(perf_obj)
	@if [ $(VERBOSE) ]; then echo $(CXX) -o perf $(perf_obj) $(LF); else echo $(CXX) -o perf; fi
	@$(CXX) -o perf $(perf_obj) $(LF)
## -------------------------------------------------------------------------------------------------
## Build individual files and track dependencies
$(TARGET_DIR)/%.o: %.cpp
	$(BUILD_DIR)
	@if [ $(VERBOSE) ]; then echo $(CXX) -MD -c -o $@ $< $(CF); else echo $(CXX) $@; fi
	@$(CXX) -MD -c -o $@ $< $(CF)
	@cp $(TARGET_DIR)/$*.d $(TARGET_DIR)/$*.P; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $(TARGET_DIR)/$*.d >> $(TARGET_DIR)/$*.P; \
		rm -f $(objDir)$*.d
## -------------------------------------------------------------------------------------------------
## Clean up the hole mess
clean:
	rm -rf $(TARGET_DIR)
	rm -f coordinator perf tester
## -------------------------------------------------------------------------------------------------
