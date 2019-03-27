#GCC Compilers:
CC  = gcc
CFLAGS   = -Ofast -fopenmp -std=c99 -lm 
LIBS     = -lm

TARGET_1 = driverForGraphClustering
TARGET = $(TARGET_1)

all: $(TARGET)

$(TARGET_1):
	$(CC) $(CFLAGS) -o $(TARGET_1) $(TARGET_1).c $(LIBS)

clean:
	rm -f $(TARGET)
