MAKE        = make
MAKE_DIRS   = scripts

.PHONY: all $(MAKE_DIRS)

all: $(MAKE_DIRS)

$(MAKE_DIRS):
	${MAKE} -C $@
