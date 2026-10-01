#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../frogui/i18n.c"
int main(void) {
    assert(i18n_init("en_US"));assert(english_count>250);
    assert(i18n_init("pl_PL"));assert(!strcmp(i18n_label("Files"),"Pliki"));
    assert(!strcmp(i18n_label("My game title"),"My game title"));
    assert(i18n_init("fr_FR"));assert(!strcmp(i18n_label("Game Details"),"Détails du jeu"));
    assert(!i18n_init("missing"));assert(!strcmp(i18n_label("Files"),"Files"));
    assert(!i18n_init("../../oops"));
    puts("PASS: UI label translation, exact fallback, locale validation");return 0;
}
