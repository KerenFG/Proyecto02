CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude -Isrc -g
LDFLAGS =

SRC_DIR  = src
OBJ_DIR  = build
TEST_DIR = tests



SRCS = \
	$(SRC_DIR)/json_parser.c    \
	$(SRC_DIR)/index_engine.c   \
	$(SRC_DIR)/jnx_io.c         \
	$(SRC_DIR)/regex_engine.c   \
	$(SRC_DIR)/json_extractor.c \
	$(SRC_DIR)/libjsonindex.c   \
	$(SRC_DIR)/main.c

OBJS   = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = jqindex



TEST_SRCS = \
	$(SRC_DIR)/json_parser.c    \
	$(SRC_DIR)/index_engine.c   \
	$(SRC_DIR)/jnx_io.c         \
	$(SRC_DIR)/regex_engine.c   \
	$(SRC_DIR)/json_extractor.c \
	$(SRC_DIR)/libjsonindex.c   \
	$(TEST_DIR)/unit_tests.c

TEST_BIN = $(TEST_DIR)/unit_tests



.PHONY: all clean test test-unit test-int

all: $(OBJ_DIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)



test: test-unit test-int

test-unit: $(TEST_BIN)
	@echo ""
	@echo "=== Unit Tests ==="
	./$(TEST_BIN)

test-int: $(TARGET)
	@echo ""
	@echo "=== Integration Tests ==="
	bash $(TEST_DIR)/run_tests.sh

$(TEST_BIN): $(TEST_SRCS)
	$(CC) $(CFLAGS) -I$(TEST_DIR) -o $@ $^



clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_BIN) $(TEST_DIR)/data/*.jnx
