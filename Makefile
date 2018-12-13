obj=main.o push.o
out=a.out

$(out):$(obj)
	g++ -o $(out) $(obj) -lpthread -lm
	rm $(obj)

main.o:main.cpp push.h
	g++ -c main.cpp

push.o:push.cpp push.h
	g++ -c push.cpp

.PHONY : clean
clean:
	rm -f $(out) $(obj)
	rm -f log