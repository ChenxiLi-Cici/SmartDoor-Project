#include "admin_menu.h"
#include <stdio.h>

void admin_menu_enter(void) {
    printf(" Menu Enter\r\n");
}

void admin_menu_handle_key(char key) {
    printf("Key '%c' pressed\r\n", key);
}

void admin_menu_exit(void) {
    printf("Menu Exited.\r\n");
}
