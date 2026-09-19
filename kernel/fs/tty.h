#pragma once

#include "../../include/vfs.h"

struct thread;

void tty_init(void);
void tty_receive_char(char c);

struct vnode* tty_get_vnode(int tty_num);
void tty_remove_waiter(struct thread *t);

