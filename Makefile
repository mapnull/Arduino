#############################################################################
#
# Makefile for MySensors
#
# Description:
# ------------
# use make all and make install to install the gateway
#

CONFIG_FILE=Makefile.inc

include $(CONFIG_FILE)

GATEWAY=examples_RPi/PiGateway
GATEWAY_SOURCES=examples_RPi/PiGateway.cpp $(wildcard ./utility/*.cpp)
GATEWAY_OBJECTS=$(patsubst %.cpp,%.o,$(GATEWAY_SOURCES))
DEPS+=$(patsubst %.cpp,%.d,$(GATEWAY_SOURCES))

CINCLUDE=-I. -I./core -I$(RF24H_LIB_DIR)

.PHONY: all gateway cleanconfig clean install uninstall force

all: $(GATEWAY)

# Gateway Build
$(GATEWAY): $(GATEWAY_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(GATEWAY_OBJECTS)

# Include all .d files
-include $(DEPS)
	
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CINCLUDE) -MMD -c -o $@ $<

# clear configuration files
cleanconfig:
	@echo "[Cleaning configuration]"
	rm -rf $(CONFIG_FILE)

# clear build files
clean:
	@echo "[Cleaning]"
	rm -rf build $(GATEWAY_OBJECTS) $(GATEWAY) $(DEPS)

$(CONFIG_FILE):
	@echo "[Running configure]"
	@./configure --no-clean

install: all install-gateway install-initscripts

install-gateway: 
	@echo "Installing $(GATEWAY) to $(EXAMPLES_DIR)"
	@install -m 0755 $(GATEWAY) $(EXAMPLES_DIR)

install-initscripts:
	