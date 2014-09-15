CC = gcc

LL = -lrt
CC_FLAGS = -Wall -Wextra -Werror

EXEC = client server

BUILD_TYPES = debug release
ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(BUILD_TYPES))))
	CC += -O2 -DENABLE_LOG
endif

all: $(EXEC)

debug:   CC_FLAGS += -DDEBUG -g -O0
debug:   $(EXEC)

release: CC_FLAGS += -O3
release: $(EXEC)

$(EXEC): %: %.c
	 $(CC) $(CC_FLAGS) $^ -o $@ $(LL)

.PHONY:  clean

clean:
	 rm $(EXEC)