## -------------------------------------------------------------------------------------------------
## Define constants
all: alex

TARGET_DIR := bin

CF := -g3 -O2 -std=c++14 -Wextra -Wall -I./src
LF := -g3 -O2 -std=c++14 -libverbs -lpthread -lzmq

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
## Grep all files relevant for the build (= all files located in sub-folders of the src folder)
src_obj := $(patsubst src/%,$(TARGET_DIR)/src/%, $(patsubst %.cpp,%.o,$(shell find `find 'src/' -maxdepth 1 -type d | tail -n +2` -name "*.cpp")))
## -------------------------------------------------------------------------------------------------
## Build the test program
tester_obj := $(src_obj) bin/src/Tester.o
alex: $(tester_obj)
	@if [ $(VERBOSE) ]; then echo $(CXX) -o tester $(tester_obj) $(LF); else echo $(CXX) -o tester; fi
	@$(CXX) -o tester $(tester_obj) $(LF)
## -------------------------------------------------------------------------------------------------
## Build the coordinator for exchanging rdma keys
coordinator_obj := $(src_obj) bin/src/Coordinator.o
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
## -------------------------------------------------------------------------------------------------
