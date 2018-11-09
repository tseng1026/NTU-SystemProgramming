all: randomforest.c
	gcc random_forest.c -o random_forest -pthread

clean:
	rm -f random_forest submission.csv

run:
	./random_forest -data ../data -output submission.csv -tree 100 -thread 5
