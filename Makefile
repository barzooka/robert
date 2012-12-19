CC=g++
OUT=robert
SRC=robert.cpp \
    getevent.cpp \
    sendevent.cpp
all:
	${CC} ${SRC} -o ${OUT}
clean:
	rm -rf *.o ${OUT}

