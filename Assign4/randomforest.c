#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define swap(a, b){float *tmp; tmp = a; a = b; b = tmp;}
#define wrong(){printf("Something has gone wrong!\n"); exit(1);}

int check = 0;
int trainid = -1;
int judgeid = -1;
float trainset[25200][35] = {{0}};
float judgeset[25200][35] = {{0}};
pthread_mutex_t reslock = PTHREAD_MUTEX_INITIALIZER;

struct node{
	int res;
	int dim;
	float thr;
	struct node *left;
	struct node *right;
};

void sort(int dim, int L, int R, float *usingset[500])
{
	// sort according the dimension
	for (int i = L; i < R; i ++)
		for (int j = i + 1; j < R; j ++)
			if (usingset[i][dim] > usingset[j][dim]) swap(usingset[i], usingset[j]);
	return;
}

void train (struct node *root, int L, int R, int usingid, float *usingset[500])
{
	// record the number of two result
	int sum = R - L, total[2] = {0};
	for (int k = L; k < R; k ++)
		total[(int)usingset[k][34]] ++;

	// if the number of one result equals the sum then it is the result
	if (sum == total[0]){root->left = root->right = NULL; root->res = 0; return;}
	if (sum == total[1]){root->left = root->right = NULL; root->res = 1; return;}

	int M = 0;
	float min = (float)2;
	for (int d = 1; d < 34; d ++){
		sort(d, L, R, usingset);

		int numL = 0, numR = sum, count[2] = {0};
		for (int k = L; k < R; k ++){
			// count the number of two result in the left tree
			count[(int)usingset[k][34]] ++;
			numL ++;
			numR --;

			if (k != usingid - 1 && usingset[k][d] == usingset[k + 1][d]) continue;

			int giniL = numL * numL;
			for (int c = 0; c < 2; c ++)
				giniL -= count[c] * count[c];

			int giniR = numR * numR;
			for (int c = 0; c < 2; c ++)
				giniR -= (total[c] - count[c]) * (total[c] - count[c]);

			// calculate the gini impurity and update min
			float gini = ((float)giniL / numL + (float)giniR / numR) / sum;
			if (gini < min){
				M = k;
				min = gini;
				root->dim = d;
				root->thr = usingset[k][d];
			}
		}
	}

	// keep doing the same thing
	sort(root->dim, L, R, usingset);
	root->left = (struct node*)malloc(sizeof(struct node));
	root->right = (struct node*)malloc(sizeof(struct node));
	root->left->res = root->left->dim = root->left->thr = 0;
	root->right->res = root->right->dim = root->right->thr = 0;
	root->left->left = root->left->right = NULL;
	root->right->left = root->right->right = NULL;
	train(root->left, L, M + 1, usingid, usingset);
	train(root->right, M + 1, R, usingid, usingset);
	return;
}

void judge (struct node *root, float *triedset)
{
	// if it is the node then root->res is the answer
	if (root->left == NULL && root->right == NULL){
		// pthread_mutex_trylock(&reslock);
		// pthread_mutex_lock(&reslock);
		triedset[34] += (float)(2 * (root->res) - 1);
		// pthread_mutex_unlock(&reslock);
		return;
	}
	else if (triedset[root->dim] <= root->thr) judge(root->left, triedset);
	else if (triedset[root->dim] > root->thr) judge(root->right, triedset);
}

void freed (struct node *root)
{
	// if it is the node then root->res is the answer
	if (root->left == NULL && root->right == NULL){
		free(root);
		return;
	}
	if (root->left == NULL) freed(root->left);
	if (root->right == NULL) freed(root->right);
}

void *thread_function(void* argv)
{
	int usingid = 500;
	float *usingset[500];
	struct node *root = (struct node*)malloc(sizeof(struct node));
	root->res = root->dim = root->thr = 0;
	root->left = root->right = NULL;

	// build the decision tree
	srand((unsigned)time(NULL));
	for (int k = 0; k < usingid; k ++)
		usingset[k] = trainset[rand() % trainid];
	train(root, 0, usingid, usingid, usingset);

	// check the result of decision tree
	for (int k = 0; k < judgeid; k ++)
		judge(root, judgeset[k]);

	// free the unuse memory
	freed(root);
	return 0;
}

int main (int argc, char** argv)
{
	if (argc != 9) wrong();
	// read from the arguments
	char trainfile[30] = {'\0'};
	char judgefile[30] = {'\0'};
	char outputfile[30] = {'\0'};
	sprintf(trainfile, "%s/training_data", argv[2]);
	sprintf(judgefile, "%s/testing_data", argv[2]);
	sprintf(outputfile, "%s", argv[4]);
	int tree = atoi(argv[6]);
	int thread = atoi(argv[8]);

	// open the necessary files
	FILE* trainfd = fopen(trainfile, "r");
	FILE* judgefd = fopen(judgefile, "r");
	FILE* outputfd = fopen(outputfile, "w+");
	if (!trainfd || !judgefd || !outputfd) wrong();

	// read from the training data
	while (1) {
		trainid ++;
		for (int k = 0; k < 35; k ++)
			if (fscanf(trainfd, "%f", &trainset[trainid][k]) == EOF) goto reading_successful_train;
	}
	reading_successful_train:;

	// read from the judgeing data
	while (1) {
		judgeid ++;
		for (int k = 0; k < 34; k ++)
			if (fscanf(judgefd, "%f", &judgeset[judgeid][k]) == EOF) goto reading_successful_judge;
	}
	reading_successful_judge:;

	// create tree
	struct node decision_tree[500];

	// create thread when thread <= tree (build tree)
	pthread_t tid[100];
	for (int k = 0; k < thread && k < tree; k ++){
		int ret = pthread_create(&tid[k], NULL, thread_function, (void*)&decision_tree[k]);
		if (ret != 0) wrong();
	}

	// use thread when thread > tree (build tree)
	for (int k = thread; k < tree; k ++){
		pthread_join(tid[k % thread], NULL);
		int ret = pthread_create(&tid[k % thread], NULL, thread_function, (void*)&decision_tree[k]);
		if (ret != 0) wrong();
	}

	// wait for thread until finishing
	for (int k = tree % thread; k < thread; k ++)
		pthread_join(tid[k], NULL);

	fprintf(outputfd, "Id,Label\n");
	for (int k = 0; k < judgeid; k ++){
		if (judgeset[k][34] > 0) fprintf(outputfd, "%d, 1\n", (int)judgeset[k][0]);
		if (judgeset[k][34] <= 0) fprintf(outputfd, "%d, 0\n", (int)judgeset[k][0]);
	}

	// close the unnecessary files
	fclose(trainfd);
	fclose(judgefd);
	fclose(outputfd);
	return 0;
}