GXX = g++
CLIENT_CPP = client.cpp
SERVER_CPP = server.cpp
CLIENT = client
SERVER = server
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv4`
all: server client


server: $(SERVER_CPP)
	$(GXX) $(SERVER_CPP) -o $(SERVER) $(INCLUDE_OPENCV)
client: $(CLIENT_CPP)
	$(GXX) $(CLIENT_CPP) -o $(CLIENT) $(INCLUDE_OPENCV)

clean: 
	rm -rf ./server ./client server_dir client_dir