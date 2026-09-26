#include <assert.h>
#include "../apps/state_slot_history.h"
int main(void) {
    const char *path="/tmp/switchfrog-state-slot-test";
    remove(path);
    assert(sf_state_slot_read(path,0,9)==-1);
    assert(sf_state_slot_write(path,8)==0);
    assert(sf_state_slot_read(path,0,9)==8);
    assert(sf_state_slot_write(path,1)==0);
    assert(sf_state_slot_read(path,0,9)==1);
    assert(sf_state_slot_write(path,0)==0);
    assert(sf_state_slot_read(path,0,9)==0);
    assert(sf_state_slot_read(path,1,10)==-1);
    assert(sf_state_slot_write(path,10)==0);
    assert(sf_state_slot_read(path,1,10)==10);
    assert(sf_state_slot_read(path,0,9)==-1);
    FILE *f=fopen(path,"w"); assert(f); fputs("invalid\n",f); fclose(f);
    assert(sf_state_slot_read(path,0,9)==-1);
    remove(path);
    puts("PASS: newest saved slot independent of clock or slot number; invalid metadata rejected");
    return 0;
}
