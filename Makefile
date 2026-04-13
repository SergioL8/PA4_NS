all: dfc dfs

dfc: dfc.c # dfc_maps.c
# 	gcc -Wall -Wextra -o dfc dfc.c dfc_maps.c -lssl -lcrypto
	gcc -Wall -Wextra -o dfc dfc.c

dfs: dfs.c
	gcc -Wall -Wextra -o dfs dfs.c

clean:
	rm -f dfc dfs *.o
	rm -rf dfs1/*
	rm -rf dfs2/*
	rm -rf dfs3/*
	rm -rf dfs4/*
