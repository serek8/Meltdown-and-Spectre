CFLAGS += -O0
CFLAGS += -Iinclude
CFLAGS += -D_GNU_SOURCE=1

LDFLAGS += -no-pie

# Change this to your own VUNETID
VUNETID = jsi510

all: check $(VUNETID)-meltdown $(VUNETID)-spectre

check:
ifndef VUNETID
		$(error VUNETID is undefined. Please specify it in the Makefile.)
endif

$(VUNETID)-meltdown: main-meltdown.o
	${CC} $< -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS}

$(VUNETID)-spectre: main-spectre.o
	${CC} $< -o $@ ${LDFLAGS} ${CFLAGS} ${LIBS}

%.o: %.c
	${CC} -c $< -o $@ ${CFLAGS} -MT $@ -MMD -MP -MF $(@:.o=.d)

clean: check
	rm -f $(VUNETID)-* *.o *.d
