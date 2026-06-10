#include "../../../doomgeneric/doomgeneric.h"

int main(void) {
    doomgeneric_Create(0, 0);
    while (1) {
        doomgeneric_Tick();
    }
    return 0;
}
