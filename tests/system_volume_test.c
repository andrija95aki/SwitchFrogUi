#include <assert.h>
#include <stdio.h>
#include "../apps/system_volume.h"
int main(void) {
    assert(sf_volume_gain(-9)==0 && sf_volume_gain(999)==100);
    for(int p=1;p<=100;p++)assert(sf_volume_gain(p)>=sf_volume_gain(p-1));
    for(int p=5;p<=100;p+=5)assert(sf_volume_gain(p)>sf_volume_gain(p-5));
    assert(sf_volume_gain(50)==13);
    puts("PASS: retained perceptual volume curve, monotonic and clamped");return 0;
}
