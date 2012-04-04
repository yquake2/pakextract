# The compiler
CC := gcc

# ----------

# Base CFLAGS
CFLAGS := -O2 -Wall -ansi -pedantic

# ----------

# When make is invoked by "make VERBOSE=1" print
# the compiler and linker commands.
ifdef VERBOSE
Q :=
else
Q := @
endif

# ----------

# The converter rule
%.o: %.c
	@echo '===> CC $<'
	${Q}$(CC) -c $(CFLAGS) -o $@ $<

# ----------

PAK_OBJS = \
	pakextract.o

# ----------

pakextract: $(PAK_OBJS)
	@echo '===> LD $@'
	${Q}$(CC) $(PAK_OBJS) -o $@
               
# ----------

clean:
	@echo "===> CLEAN"
	${Q}rm -Rf pakextract.o pakextract

