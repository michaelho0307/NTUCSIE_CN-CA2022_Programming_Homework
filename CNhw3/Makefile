CC = g++

S_CPP = sender.cpp
A_CPP = agent.cpp
R_CPP = receiver.cpp
S_B = sender
A_B = agent
R_B = receiver
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv4`

all: agent sender receiver

agent:
	$(CC) $(A_CPP) -o $(A_B) -lz
sender:
	$(CC) $(S_CPP) -o $(S_B) $(INCLUDE_OPENCV) -lz
receiver:
	$(CC) $(R_CPP) -o $(R_B) $(INCLUDE_OPENCV) -lz

clean:
	rm $(A_B) $(S_B) $(R_B)