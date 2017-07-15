sever:sever.c client.c
	gcc sever.c -lpthread  -o sever
	gcc client.c -lpthread -o client
clean:
	rm -rf ./*.o ./*.out 
