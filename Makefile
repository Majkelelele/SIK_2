CC=g++
CFLAGS=-Wall -Wextra -std=c++11
LDFLAGS=

SRCS_SERVER=server.cpp server-func.cpp err.cpp common.cpp
SRCS_CLIENT=client.cpp client-func.cpp err.cpp common.cpp

OBJS_SERVER=$(SRCS_SERVER:.cpp=.o)
OBJS_CLIENT=$(SRCS_CLIENT:.cpp=.o)

EXEC_SERVER=server
EXEC_CLIENT=client

.PHONY: all clean

all: $(EXEC_SERVER) $(EXEC_CLIENT)

$(EXEC_SERVER): $(OBJS_SERVER)
	$(CC) $(LDFLAGS) -o $@ $^

$(EXEC_CLIENT): $(OBJS_CLIENT)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS_SERVER) $(OBJS_CLIENT) $(EXEC_SERVER) $(EXEC_CLIENT)
