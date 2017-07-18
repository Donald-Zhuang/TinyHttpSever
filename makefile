sever:sever.c client.c
	gcc sever.c -lpthread -Wall -g -o sever
	gcc client.c -lpthread -Wall -g -o client
clean:
	rm -rf ./*.o ./*.out 
