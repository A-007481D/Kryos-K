#pragma once

#include "../../include/vfs.h"

void tty_init(void);
void tty_receive_char(char c);

extern vnode_ops_t tty_vnode_ops;
extern struct vnode tty_vnode;
