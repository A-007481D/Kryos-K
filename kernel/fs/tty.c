#include "tty.h"
#include "../../include/thread.h"
#include "../../include/serial.h"
#include "../../kernel/memory/virt.h"
#include "../../include/heap.h"
#include "../../kernel/lib/irq.h"
#include "vt.h"
#include <string.h>

#define TTY_LINE_MAX 256
#define MAX_TTYS MAX_VTS

struct tty {
    char buf[TTY_LINE_MAX];
    int len;
    struct thread *wait_queue;
    struct vnode vnode;
};

static struct tty ttys[MAX_TTYS];
static vnode_ops_t tty_vnode_ops;

void tty_init(void) {
    vt_init();
    for (int i = 0; i < MAX_TTYS; i++) {
        ttys[i].len = 0;
        ttys[i].wait_queue = NULL;
        ttys[i].vnode.ops = &tty_vnode_ops;
        ttys[i].vnode.fs_private = (void *)(uintptr_t)i;
        ttys[i].vnode.type = VNODE_TYPE_FILE;
    }
}

struct vnode* tty_get_vnode(int tty_num) {
    if (tty_num < 0 || tty_num >= MAX_TTYS) return NULL;
    return &ttys[tty_num].vnode;
}

void tty_receive_char(char c) {
    irq_state_t flags = irq_save();
    
    int active_idx = vt_get_active();
    struct tty *t = &ttys[active_idx];
    
    if (c == '\b') {
        if (t->len > 0 && t->buf[t->len - 1] != '\n') {
            t->len--;
            vt_write(active_idx, "\b \b", 3);
        }
    } else if (c == '\n') {
        if (t->len < TTY_LINE_MAX) {
            t->buf[t->len++] = '\n';
            vt_write(active_idx, "\n", 1);
            
            // Wake all waiting threads
            struct thread *curr = t->wait_queue;
            while (curr) {
                struct thread *next = curr->next_waiter;
                curr->state = THREAD_READY;
                curr->next_waiter = NULL;
                curr = next;
            }
            t->wait_queue = NULL;
        }
    } else {
        if (t->len < TTY_LINE_MAX) {
            t->buf[t->len++] = c;
            char s[1] = {c};
            vt_write(active_idx, s, 1);
        }
    }
    
    irq_restore(flags);
}

static int tty_read_vfs(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read) {
    (void)f;
    int tty_num = (int)(uintptr_t)vn->fs_private;
    struct tty *t = &ttys[tty_num];
    char *kbuf = (char *)buf;
    
    irq_state_t flags = irq_save();
    
    while (1) {
        int nl_pos = -1;
        for (int i = 0; i < t->len; i++) {
            if (t->buf[i] == '\n') {
                nl_pos = i;
                break;
            }
        }
        
        if (nl_pos != -1) {
            // We have a complete line
            size_t to_copy = count;
            if (to_copy > (size_t)(nl_pos + 1)) {
                to_copy = nl_pos + 1;
            }
            
            memcpy(kbuf, t->buf, to_copy);
            *bytes_read = to_copy;
            
            t->len -= to_copy;
            if (t->len > 0) {
                // memmove manually
                for (int i = 0; i < t->len; i++) {
                    t->buf[i] = t->buf[i + to_copy];
                }
            }
            
            irq_restore(flags);
            return 0; // Success
        }
        
        // No complete line, enqueue and block
        struct thread *curr = thread_current();
        curr->next_waiter = t->wait_queue;
        t->wait_queue = curr;
        
        curr->state = THREAD_BLOCKED;
        schedule();
        
        irq_restore(flags);
        flags = irq_save();
    }
}

static int tty_write_vfs(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *bytes_written) {
    (void)f;
    int tty_num = (int)(uintptr_t)vn->fs_private;
    if (!buf || !bytes_written) return -EINVAL;
    
    vt_write(tty_num, (const char *)buf, count);
    *bytes_written = count;
    return 0;
}

static void tty_close_vfs(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
}

static vnode_ops_t tty_vnode_ops = {
    .read = tty_read_vfs,
    .write = tty_write_vfs,
    .close = tty_close_vfs
};

void tty_remove_waiter(struct thread *t) {
    for (int i = 0; i < MAX_TTYS; i++) {
        struct tty *tty = &ttys[i];
        if (tty->wait_queue == t) {
            tty->wait_queue = t->next_waiter;
        } else {
            struct thread *tw = tty->wait_queue;
            while (tw && tw->next_waiter) {
                if (tw->next_waiter == t) {
                    tw->next_waiter = t->next_waiter;
                    break;
                }
                tw = tw->next_waiter;
            }
        }
    }
}

