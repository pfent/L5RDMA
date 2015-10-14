## -------------------------------------------------------------------------------------------------
## Define constants
all: alex

TARGET_DIR := bin

CF := -g3 -O2 -std=c++14 -Wextra -Wall -I./src
LF := -g3 -O2 -std=c++14 -libverbs -lpthread

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
## Grep all files relevant for the build
src_obj  := $(patsubst src/%,$(TARGET_DIR)/src/%, $(patsubst %.cpp,%.o,$(shell find src -name "*.cpp")))
## -------------------------------------------------------------------------------------------------
## Build the test program
alex: $(src_obj) bin/tester.o
	@if [ $(VERBOSE) ]; then echo $(CXX) -o tester bin/tester.o $(src_obj) $(LF); else echo $(CXX) -o tester; fi
	@$(CXX) -o tester bin/tester.o $(src_obj) $(LF)
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
