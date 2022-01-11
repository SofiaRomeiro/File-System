# Makefile, v1
# Sistemas Operativos, DEI/IST/ULisboa 2021-22
#
# This makefile should be run from the *root* of the project

CC ?= gcc
LD ?= gcc

# space separated list of directories with header files
INCLUDE_DIRS := fs .
# this creates a space separated list of -I<dir> where <dir> is each of the values in INCLUDE_DIRS
INCLUDES = $(addprefix -I, $(INCLUDE_DIRS))

SOURCES  := $(wildcard */*.c)
HEADERS  := $(wildcard */*.h)
OBJECTS  := $(SOURCES:.c=.o)
TARGET_EXECS := tests/tfs_built_in_functions tests/copy_to_external_simple tests/copy_to_external_short_str tests/copy_to_external_huge_file tests/copy_to_external_direct_blocks tests/copy_to_external_indirect_blocks tests/copy_to_external_int tests/copy_to_external_errors tests/write_10_blocks_spill tests/write_10_blocks_simple tests/write_more_than_10_blocks_simple tests/write_more_than_10_blocks_spill tests/write_all_direct_blocks tests/write_more_than_1_block tests/write_max_bytes tests/max_storage_exceeded tests/copy_to_external_different_char tests/copy_to_external_multiple_char

# VPATH is a variable used by Makefile which finds *sources* and makes them available throughout the codebase
# vpath %.h <DIR> tells make to look for header files in <DIR>
vpath # clears VPATH
vpath %.h $(INCLUDE_DIRS)

CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L
CFLAGS += $(INCLUDES)

#thread flags 
LDFLAGS = -lpthread
LDFLAGS += $(INCLUDES)

# Warnings
CFLAGS += -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused
# Warning suppressions
CFLAGS += -Wno-sign-compare

# optional debug symbols: run make DEBUG=no to deactivate them
ifneq ($(strip $(DEBUG)), no)
  CFLAGS += -g
endif

# optional O3 optimization symbols: run make OPTIM=no to deactivate them
ifeq ($(strip $(OPTIM)), no)
  CFLAGS += -O0
else
  CFLAGS += -O3
endif

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean depend fmt

all: clean $(TARGET_EXECS)

valgrind :
	@echo ------- Starting Valgrind -------
	valgrind -s --tool=memcheck --leak-check=full --track-origins=yes ./tests/write_10_blocks_simple

test : test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16 test17 test18
	@echo "Ending tests :)"

test1:
	@echo ----- Test 1 ------
	./tests/tfs_built_in_functions

test2:
	@echo ----- Test 2 ------
	./tests/copy_to_external_simple

test3:
	@echo ----- Test 3 ------
	./tests/copy_to_external_errors

test4:
	@echo ----- Test 4 ------
	./tests/write_10_blocks_simple

test5:
	@echo ----- Test 5 ------
	./tests/write_more_than_10_blocks_simple

test6:
	@echo ----- Test 6 ------
	./tests/write_10_blocks_spill

test7:
	@echo ----- Test 7 ------
	./tests/copy_to_external_int

test8:
	@echo ----- Test 8 ------
	./tests/copy_to_external_huge_file

test9:
	@echo ----- Test 9 ------
	./tests/copy_to_external_direct_blocks

test10:
	@echo ----- Test 10 ------
	./tests/copy_to_external_indirect_blocks

test11:
	@echo ----- Test 11 ------
	./tests/write_more_than_10_blocks_spill

test12:
	@echo ----- Test 12 ------
	./tests/write_more_than_1_block

test13:
	@echo ----- Test 13 ------
	./tests/write_all_direct_blocks

test14:
	@echo ----- Test 14 ------
	./tests/write_max_bytes

test15:
	@echo ----- Test 15 ------
	./tests/max_storage_exceeded

test16:
	@echo ----- Test 16 ------
	./tests/copy_to_external_short_str

test17:
	@echo ----- Test 17 ------
	./tests/copy_to_external_different_char

test18:
	@echo ----- Test 18 ------
	./tests/copy_to_external_multiple_char

# The following target can be used to invoke clang-format on all the source and header
# files. clang-format is a tool to format the source code based on the style specified 
# in the file '.clang-format'.
# More info available here: https://clang.llvm.org/docs/ClangFormat.html

# The $^ keyword is used in Makefile to refer to the right part of the ":" in the 
# enclosing rule. See https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

fmt: $(SOURCES) $(HEADERS)
	clang-format -i $^



# Note the lack of a rule.
# make uses a set of default rules, one of which compiles C binaries
# the CC, LD, CFLAGS and LDFLAGS are used in this rule
tests/tfs_built_in_functions: tests/tfs_built_in_functions.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_errors: tests/copy_to_external_errors.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_simple: tests/copy_to_external_simple.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_int: tests/copy_to_external_int.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_short_str: tests/copy_to_external_short_str.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_huge_file: tests/copy_to_external_huge_file.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_direct_blocks: tests/copy_to_external_direct_blocks.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_indirect_blocks: tests/copy_to_external_indirect_blocks.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_10_blocks_spill: tests/write_10_blocks_spill.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_10_blocks_simple: tests/write_10_blocks_simple.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_more_than_10_blocks_simple: tests/write_more_than_10_blocks_simple.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_more_than_10_blocks_spill: tests/write_more_than_10_blocks_spill.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_all_direct_blocks: tests/write_all_direct_blocks.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_more_than_1_block: tests/write_more_than_1_block.o fs/operations.o fs/state.o fs/aux_operations.o
tests/write_max_bytes: tests/write_max_bytes.o fs/operations.o fs/state.o fs/aux_operations.o
tests/max_storage_exceeded: tests/max_storage_exceeded.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_different_char: tests/copy_to_external_different_char.o fs/operations.o fs/state.o fs/aux_operations.o
tests/copy_to_external_multiple_char: tests/copy_to_external_multiple_char.o fs/operations.o fs/state.o fs/aux_operations.o

clean:
	rm -f $(OBJECTS) $(TARGET_EXECS)




# This generates a dependency file, with some default dependencies gathered from the include tree
# The dependencies are gathered in the file autodep. You can find an example illustrating this GCC feature, without Makefile, at this URL: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/GCC/options/MM
# Run `make depend` whenever you add new includes in your files
depend : $(SOURCES)
	$(CC) $(INCLUDES) -MM $^ > autodep
