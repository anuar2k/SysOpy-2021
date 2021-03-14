NAME   := library_test
CC     := gcc
CFLAGS := -Wall -g
OPT    ?= O2

.PHONY: all clean run_tests

all: $(NAME)_static_$(OPT) $(NAME)_shared_$(OPT) $(NAME)_dynamic_$(OPT)

$(NAME)_static_$(OPT): $(NAME).c
	$(CC) $(CFLAGS) -$(OPT) -static $(NAME).c -I../zad01/rrmerge -L../zad01/rrmerge -lrrmerge -o $(NAME)_static_$(OPT)

$(NAME)_shared_$(OPT): $(NAME).c
	$(CC) $(CFLAGS) -$(OPT) $(NAME).c -lrrmerge -o $(NAME)_shared_$(OPT)

$(NAME)_dynamic_$(OPT): $(NAME).c
	$(CC) $(CFLAGS) -$(OPT) $(NAME).c -ldl -DDYNAMIC -o $(NAME)_dynamic_$(OPT)

run_tests: $(NAME)_static_$(OPT) $(NAME)_shared_$(OPT) $(NAME)_dynamic_$(OPT)
	echo "running tests for static"
	./$(NAME)_static_$(OPT) --quiet < test/test_commands.txt | tee $(NAME)_static_$(OPT)_result.txt
	echo "running tests for shared"
	./$(NAME)_shared_$(OPT) --quiet < test/test_commands.txt | tee $(NAME)_shared_$(OPT)_result.txt
	echo "running tests for dynamic"
	./$(NAME)_dynamic_$(OPT) --quiet < test/test_commands.txt | tee $(NAME)_dynamic_$(OPT)_result.txt

clean:
	$(RM) $(NAME)_static_* $(NAME)_shared_* $(NAME)_dynamic_*
