NAME    :=  rrmerge
SNAME   :=  $(NAME).a
DNAME   :=  $(NAME).so
SRC     :=  $(wildcard *.c)
OBJ     :=  $(SRC:.c=.o)
CFLAGS  :=  -Wall -g3 -fPIC
LDFLAGS :=  -L.
LDLIBS  :=  -l$(...)

.PHONY: all clean fclean re

all: $(SNAME) $(DNAME)

$(SNAME): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(DNAME): LDFLAGS += -shared
$(DNAME): $(OBJ)
    $(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
    $(RM) $(OBJ)

fclean: clean
    $(RM) $(SNAME) $(DNAME)

re: fclean all