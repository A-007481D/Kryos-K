#ifndef KRYOS_TESTS_H
#define KRYOS_TESTS_H

void run_kernel_tests(void);
void test_vmm_suite(void);
void test_heap_suite(void);
void test_thread_suite(void);
void test_preempt_suite(void);
void test_user_suite(void);
void test_process_suite(void);
void test_process_hierarchy_suite(void);
void test_fs_suite(void);
void test_kfs_suite(void);
void test_vfs_suite(void);
void test_syscall_fs_suite(void);

#endif // KRYOS_TESTS_H
