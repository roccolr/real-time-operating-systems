#include <sys/time.h>
#include <stdio.h>

int main(){
	struct timeval tv;
	struct timezone tz;
	int res = gettimeofday(&tv, &tz);

	printf("[main]\tresult%d\tsince the epoch: seconds[%ld]\tuseconds[%06ld]\n", res, tv.tv_sec, tv.tv_usec);
	return 0;
}
