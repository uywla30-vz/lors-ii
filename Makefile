# Makefile for Lors Language

PYTHON = python3
COMPILER = compiler.py
SOURCES = $(wildcard lors/examples/*.lr) $(wildcard lors/tests/*.lr)
EXECUTABLES = $(SOURCES:.lr=)

.PHONY: all test clean

all: $(EXECUTABLES)

# Compile .lr files to executables
# $@ is the target (executable), $< is the dependency (.lr file)
%: %.lr
	@echo "Compiling $<..."
	$(PYTHON) $(COMPILER) $<

test: all
	@echo "Running all tests..."
	@for prog in $(EXECUTABLES); do \
		echo "----------------------------------------"; \
		echo "Running $$prog"; \
		if echo "$$prog" | grep -q "test_calculator"; then \
			printf "1\n10\n20\n" | ./$$prog || exit 1; \
		elif echo "$$prog" | grep -q "test_string_input"; then \
			printf "HelloInput\n" | ./$$prog || exit 1; \
		elif echo "$$prog" | grep -q "test_cli_args_basic"; then \
			./$$prog arg1 arg2 || exit 1; \
		elif echo "$$prog" | grep -q "test_cli_args_usage"; then \
			./$$prog myargument || exit 1; \
		else \
			./$$prog || exit 1; \
		fi \
	done
	@echo "----------------------------------------"
	@echo "All tests passed successfully."

clean:
	@echo "Cleaning up..."
	rm -f $(EXECUTABLES)
	rm -f lors/examples/*.cpp lors/tests/*.cpp
