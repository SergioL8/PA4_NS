all: dfc dfs

dfc: dfc.c # dfc_maps.c
	gcc -Wall -Wextra -o dfc dfc.c -lssl -lcrypto -lm
# 	gcc -Wall -Wextra -o dfc dfc.c

dfs: dfs.c
	gcc -Wall -Wextra -o dfs dfs.c

clean:
	rm -f dfc dfs *.o
	killall dfs
