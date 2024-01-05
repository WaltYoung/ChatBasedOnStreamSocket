CXX = g++

CXXFLAGS = -g -lpthread -I ./include/mysql -lmysqlclient -I ./include/ 

CLIENT_SRCS = ./c/client.cpp
SERVER_SRCS = ./s/server.cpp

CLIENT_TARGET = ./c/client
SERVER_TARGET = ./s/server

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_SRCS)
	$(CXX) $(CLIENT_SRCS) -o $(CLIENT_TARGET) $(CXXFLAGS)

$(SERVER_TARGET): $(SERVER_SRCS)
	$(CXX) $(SERVER_SRCS) -o $(SERVER_TARGET) $(CXXFLAGS)

clean:
	rm -f $(CLIENT_TARGET) $(SERVER_TARGET)
