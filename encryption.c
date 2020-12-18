#include "visual.h"
#include <string.h>

struct thread_info{
	image img;
	image mask;
	image out;
	int size;
	int threadId;
};

void *dither_pids(void *arg) {
	struct thread_info *thread_ptr = (struct thread_info*) arg;
	int i, j, pos, errorR, errorG, errorB;
	Pixel newVal;

 	for(i = 0; i < thread_ptr->img.y; i++) {
		for(j = 0; j < thread_ptr->img.x; j++) {
			pos = thread_ptr->img.x * i + j;

			newVal = getClosestPixel(thread_ptr->img.dataColor[pos], 0);
			errorR = thread_ptr->img.dataColor[pos].r - newVal.r;
			errorG = thread_ptr->img.dataColor[pos].g - newVal.g;
			errorB = thread_ptr->img.dataColor[pos].b - newVal.b;
			thread_ptr->img.dataColor[pos] = newVal;
			correctNeighbors(thread_ptr->img.dataColor, thread_ptr->img.x, thread_ptr->img.y, j, i, errorR, errorG, errorB);
		}
	}

	return NULL;
}

void *makeMask_pids(void *arg) {
	int i, j;

	struct thread_info *thread_ptr = (struct thread_info*) arg;


	int key[4];
	key[0] = 0;
	key[1] = 128;
	key[2] = 192;
	key[3] = 255;
	int line1, line2;

	for(i = 0; i < thread_ptr->mask.y; i+= 2) {
		for(j = 0; j < thread_ptr->mask.x; j+= 2) {
			int seed = i * j;
			line1 = i * thread_ptr->mask.x + j;
			line2 = (i + 1) * thread_ptr->mask.x + j;
			key_arr(key, seed);

			thread_ptr->mask.dataGray[line1] = key[0];
			thread_ptr->mask.dataGray[line1 + 1] = key[1];

			thread_ptr->mask.dataGray[line2] = key[2];
			thread_ptr->mask.dataGray[line2 + 1] = key[3];
		}
	}

	return NULL;
}

void *encrypt_image_ids(void *arg) {
	int i, j, line1, line2, pos, k;
	int p[4];

	struct thread_info *thread_ptr = (struct thread_info*) arg;

	for(i = 0; i < thread_ptr->img.y; i++) {
		line1 = 2 * i * thread_ptr->mask.x;
		line2 = (2 * i + 1) * thread_ptr->mask.x;
		for(j = 0; j < thread_ptr->img.x; j++) {
			pos = i * thread_ptr->img.x + j;
			p[0] = line1 + 2 * j;
			p[1] = p[0] + 1;
			p[2] = line2 + 2 * j;
			p[3] = p[2] + 1;

			for(k = 0; k < 4; k++) {
				if (thread_ptr->mask.dataGray[p[k]] == thread_ptr->img.dataColor[pos].r) {
					thread_ptr->out.dataColor[p[k]].r = 255;
				}

				if(thread_ptr->mask.dataGray[p[k]] == thread_ptr->img.dataColor[pos].g) {
					thread_ptr->out.dataColor[p[k]].g = 255;
				}

				if(thread_ptr->mask.dataGray[p[k]] == thread_ptr->img.dataColor[pos].b) {
					thread_ptr->out.dataColor[p[k]].b = 255;
				}
 			}

		}
	}

	return NULL;
}

int main(int argc, char **argv) {
	int type, i, thread_count;
	image input, mask, share, out;
	struct thread_info *threadData;
	pthread_t *ids;
	if(strcmp(argv[1], "encrypt") == 0) {

		type = readInput(argv[2], &input);
		sscanf(argv[3], "%d", &thread_count);
		ids = (pthread_t *)malloc(thread_count * sizeof(pthread_t));
		mask.x = 2 * input.x;
		mask.y = 2 * input.y;
		mask.density = 1;
		mask.dataGray = (unsigned char *)malloc(mask.x * mask.y * sizeof(unsigned char));

		share.x = mask.x;
		share.y = mask.y;
		share.density = 3;
		share.dataColor = (Pixel *)malloc(share.x * share.y * sizeof(Pixel));


		int rows = input.y / thread_count;
		int chunk = rows * input.x;
		threadData = (struct thread_info *)malloc(thread_count * sizeof(struct thread_info));

		for(i = 0; i < thread_count; i++) {
			threadData[i].img.dataColor = (Pixel*)malloc(chunk * sizeof(Pixel));
			threadData[i].img.x = input.x;
			threadData[i].img.y = rows;
			threadData[i].img.density = 3;
			memcpy(threadData[i].img.dataColor, input.dataColor + chunk * i, chunk * sizeof(Pixel));
			threadData[i].size = thread_count;
			threadData[i].threadId = i;
			pthread_create(&ids[i], NULL, dither_pids, (void *)&threadData[i]);
		}

		for(i = 0; i < thread_count; i++) {
			pthread_join(ids[i], NULL);
		}

		for(i = 0; i < thread_count; i++) {
			threadData[i].mask.dataGray = (unsigned char*)malloc(chunk * 4 * sizeof(unsigned char*));
			threadData[i].mask.x = 2 * input.x;
			threadData[i].mask.y = rows * 2;
			pthread_create(&ids[i], NULL, makeMask_pids, (void *)&threadData[i]);
		}

		for(i = 0; i < thread_count; i++) {
			pthread_join(ids[i], NULL);
		}

		for(i = 0; i < thread_count; i++) {
			threadData[i].out.dataColor = (Pixel *)malloc(chunk * 4 * sizeof(Pixel));
			threadData[i].out.x = mask.x;
			threadData[i].out.y = 2 * rows;
			threadData[i].out.density = 3;
			pthread_create(&ids[i], NULL, encrypt_image_ids, (void *)&threadData[i]);
		}


		for(i = 0; i < thread_count; i++) {
			pthread_join(ids[i], NULL);
		}


		for(i = 0; i < thread_count; i++) {
			memcpy(input.dataColor + chunk * i, threadData[i].img.dataColor, chunk * sizeof(Pixel));
			memcpy(mask.dataGray + chunk * 4 * i, threadData[i].mask.dataGray, chunk * 4 * sizeof(unsigned char));
			memcpy(share.dataColor + chunk * 4 * i, threadData[i].out.dataColor, chunk * 4 * sizeof(Pixel));
		}

		writeData("mask.png", &mask, 0);
		writeData("encrypt.png", &share, 1);
	}
}
