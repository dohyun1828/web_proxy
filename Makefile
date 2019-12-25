all: web_proxy

web_proxy: main.o
	g++ -o web_proxy main.o -lpthread

clean:
	rm -f main *.o
