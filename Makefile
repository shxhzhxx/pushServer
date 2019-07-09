obj=main.o push.o
out=push-daemon

$(out):$(obj)
	g++ -o $(out) $(obj) -lpthread -lm
	rm $(obj)

main.o:main.cpp push.h
	g++ -c main.cpp -std=c++11

push.o:push.cpp push.h
	g++ -c push.cpp -std=c++11

.PHONY : clean
clean:
	rm -f $(out) $(obj)
	rm -f log