CXX = g++
INCLUDE = ../include
CXXFLAGS = -std=c++17 -Wall -I fakeheader -I $(INCLUDE) -I /usr/local/include/gtest/ -c
LXXFLAGS = -std=c++17 -I h  -pthread
OBJECTS = ./obj/main.o ./obj/pinpad_matrix_unittest.o
GTEST = /usr/lib/x86_64-linux-gnu/libgtest.a
TARGET = main


$(TARGET): $(OBJECTS)
	$(CXX) $(LXXFLAGS) -o $(TARGET) $(OBJECTS) $(GTEST)
./obj/pinpad_matrix_unittest.o: ./cpp/pinpad_matrix_unittest.cpp
	$(CXX) $(CXXFLAGS) ./cpp/pinpad_matrix_unittest.cpp -o ./obj/pinpad_matrix_unittest.o
./obj/main.o: ./cpp/main.cpp
	$(CXX) $(CXXFLAGS) ./cpp/main.cpp -o ./obj/main.o
clean:
	rm -fv $(TARGET) $(OBJECTS)

