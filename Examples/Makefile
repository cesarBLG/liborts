EXAMPLES = raildriver
ifeq ($(TARGET),WIN32)
EXAMPLES += ts
endif
all: $(EXAMPLES)
raildriver:
	$(MAKE) -C RailDriver
ts:
	$(MAKE) -C TrainSimulator
