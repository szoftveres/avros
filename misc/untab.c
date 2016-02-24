#include "stdio.h"
#include "stdlib.h"
#include "string.h"

int main (int argc, char** argv) {
    int c;
    int i;
    while ((c = getchar()) != EOF) {
        switch (c) {
          case '\t':
            printf("    ");
            break;
          default:
            printf("%c", c);
            break;
        }
    }
}
