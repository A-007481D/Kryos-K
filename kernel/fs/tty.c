#include "tty.h"
#include "../../include/thread.h"
#include "../../include/serial.h"
#include "../../kernel/memory/virt.h"
#include "../../include/heap.h"
#include "../../kernel/lib/irq.h"
#include <string.h>

#define TTY_LINE_MAX 256

static char tty_buf[TTY_LINE_MAX];
static int tty_len = 0;

struct thread *tty_wait_queue = NULL;

static void tty_echo(char c) {
    if (c == '\b') {
        serial_puts("\b \b");
    } else {
        char s[2] = {c, 0};
        serial_puts(s);
    }
}

void tty_receive_char(char c) {
    irq_state_t flags = irq_save();
    
    if (c == '\b') {
        if (tty_len > 0 && tty_buf[tty_len - 1] != '\n') {
            tty_len--;
            tty_echo('\b');
        }
    } else if (c == '\n') {
        if (tty_len < TTY_LINE_MAX) {
            tty_buf[tty_len++] = '\n';
            tty_echo('\n');
            
            // Wake all waiting threads
            struct thread *curr = tty_wait_queue;
            while (curr) {
                struct thread *next = curr->next_waiter;
                curr->state = THREAD_READY;
                curr->next_waiter = NULL;
                curr = next;
            }
            tty_wait_queue = NULL;
        }
    } else {
        if (tty_len < TTY_LINE_MAX) {
            tty_buf[tty_len++] = c;
            tty_echo(c);
        }
    }
    
    irq_restore(flags);
}

static int tty_read_vfs(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read) {
    (void)vn;
    (void)f;
    char *kbuf = (char *)buf;
    
    irq_state_t flags = irq_save();
    
    while (1) {
        int nl_pos = -1;
        for (int i = 0; i < tty_len; i++) {
            if (tty_buf[i] == '\n') {
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
            
            memcpy(kbuf, tty_buf, to_copy);
            *bytes_read = to_copy;
            
            tty_len -= to_copy;
            if (tty_len > 0) {
                // memmove manually
                for (int i = 0; i < tty_len; i++) {
                    tty_buf[i] = tty_buf[i + to_copy];
                }
            }
            
            irq_restore(flags);
            return 0; // Success
        }
        
        // No complete line, enqueue and block
        struct thread *curr = thread_current();
        curr->next_waiter = tty_wait_queue;
        tty_wait_queue = curr;
        
        curr->state = THREAD_BLOCKED;
        schedule();
        
        // Woken up, loop repeats with interrupts disabled from irq_save above?
        // Actually schedule() returns with interrupts re-enabled if we came from syscall.
        // Wait! We need to re-disable interrupts before re-checking the buffer.
        // But schedule() doesn't touch interrupts, it just swaps context.
        // If we came from syscall, context_switch will return, then irq_restore(flags) runs.
        // BUT we need to check the buffer atomically!
        // We can just do: irq_restore(flags); flags = irq_save();
        irq_restore(flags);
        flags = irq_save();
    }
}

static int tty_write_vfs(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *bytes_written) {
    (void)vn;
    (void)f;
    (void)buf;
    (void)count;
    (void)bytes_written;
    return -1; // Write to tty not supported (fd1, fd2 map to console)
}

static void tty_close_vfs(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
}

vnode_ops_t tty_vnode_ops = {
    .read = tty_read_vfs,
    .write = tty_write_vfs,
    .close = tty_close_vfs
};

struct vnode tty_vnode = {
    .ops = &tty_vnode_ops,
    .fs_private = NULL
};

void tty_init(void) {
    tty_len = 0;
    tty_wait_queue = NULL;
}
