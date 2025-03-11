#include <stdio.h>
#include <string.h>

int main() {
    char buffer[4];
    fgets(buffer, 4, stdin);

    for (int i = 0; i < 4; i++) {
        if (buffer[i] == '\n') {
            printf("char [%d]: new_line\n", i);
        } 
        else if (buffer[i] == '\0') {
            printf("char [%d]: null_term\n", i);
        }
        else {
            printf("char [%d]: %c\n", i, buffer[i]);
        }
    }
    printf("END\n\n");

    if (strcmp("ab\n", buffer) == 0) {
        printf("EQUAL\n");
    } else {
        printf("NOT\n");
    }
}
