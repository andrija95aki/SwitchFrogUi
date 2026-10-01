#include <assert.h>
#include "../apps/ps1_compat.h"
int main(void) {
    char fields[15][512],id[16];
    assert(sf_csv_fields("A,\"PAL, Europe\",\"a \"\"quote\"\"\",",fields,15)==4);
    assert(!strcmp(fields[1],"PAL, Europe"));
    assert(!strcmp(fields[2],"a \"quote\""));
    assert(sf_csv_fields("\"unterminated",fields,15)==-1);
    sf_disc_id("sles_031.37",id);assert(!strcmp(id,"SLES03137"));
    assert(sf_ps1_compat_lookup("assets/upstream-160/ps1-compatibility.csv","SLUS00402",0,fields));
    assert(!strcmp(fields[1],"USA"));
    assert(!sf_ps1_compat_lookup("assets/upstream-160/ps1-compatibility.csv","SLES00402",0,fields));
    puts("PASS: quoted CSV, disc ID and region isolation");
    return 0;
}
