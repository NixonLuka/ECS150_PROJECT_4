# Target library
lib := libfs.a

all: $(lib)

objs := disk.o fs.o


CC	:= gcc
CFLAGS	:= -Wall -Wextra -Werror
CFLAGS	+= -g
LIBFLAGS:= ar rcs

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs.a: $(objs)  # let's link library files into a static library
	@echo "AR   $@"
	$(Q)$(LIBFLAGS) $@ $^

%.o: %.c
	@echo "CC   $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	@echo "CLEAN"
	$(Q)rm -f $(targets) $(objs) $(deps)
## TODO: Phase 1
