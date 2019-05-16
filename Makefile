#GCC Compilers:
CC  = gcc
CFLAGS   = -Ofast -fopenmp -std=c99 -lm 
LIBS     = -lm

TARGET_1 = driverForGraphClustering
#TARGET = $(TARGET_1)

TARGET_2 = driverForGraphClusteringParallel
TARGET = $(TARGET_2) $(TARGET_1)

OBJECTS = RngStream.o

all: $(TARGET)

$(TARGET_1): $(OBJECTS) $(TARGET_1).o
	$(CC) $(CFLAGS) -o $(TARGET_1) $(TARGET_1).o $(OBJECTS) $(LIBS)

$(TARGET_2): $(OBJECTS) $(TARGET_2).o
	$(CC) $(CFLAGS) -o $(TARGET_2) $(TARGET_2).o $(OBJECTS) $(LIBS)

clean:
	rm -f $(TARGET)
