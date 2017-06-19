obj=main.o json.o push.o rb_tree.o
out=a.out

$(out):$(obj)
	g++ -o $(out) $(obj) -lpthread -lm
	rm $(obj)

main.o:main.cpp json.h push.h
	g++ -c main.cpp

json.o: json.cpp json.h
	g++ -c json.cpp

push.o:push.cpp push.h
	g++ -c push.cpp

rb_tree.o:rb_tree.cpp rb_tree.h
	g++ -c rb_tree.cpp

.PHONY : clean
clean:
	rm -f $(out) $(obj)
	rm -f push_log.log