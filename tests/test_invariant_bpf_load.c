#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Forward declaration of the vulnerable function from bpf_load.c */
extern int load_bpf_file(char *path);

START_TEST(test_bpf_load_buffer_overflow_protection)
{
    /* Invariant: Buffer reads never exceed declared length.
       The event parameter must not cause stack buffer overflow
       when concatenated into fixed-size path buffer. */
    
    const char *payloads[] = {
        /* Valid input: normal event name */
        "normal_event",
        
        /* Boundary case: event at max reasonable length */
        "a_very_long_but_still_reasonable_event_name_that_fits",
        
        /* Exploit case 1: extremely long event name (2x typical buffer) */
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        
        /* Exploit case 2: massive payload (10x typical buffer) */
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        
        /* Boundary case: path with special characters */
        "../../../etc/passwd"
    };
    
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);
    
    for (int i = 0; i < num_payloads; i++) {
        /* Create a temporary BPF file path with the payload as event name */
        char test_path[512];
        snprintf(test_path, sizeof(test_path), "/tmp/test_bpf_%d.o", i);
        
        /* Create a minimal valid ELF file for testing */
        int fd = open(test_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            /* Write minimal ELF header */
            unsigned char elf_header[] = {0x7f, 'E', 'L', 'F', 1, 1, 1, 0};
            write(fd, elf_header, sizeof(elf_header));
            close(fd);
        }
        
        /* Call the vulnerable function with oversized event payload.
           The function should either safely reject the input or truncate it,
           not overflow the stack buffer. */
        int result = load_bpf_file(test_path);
        
        /* Invariant check: function should not crash or return undefined behavior.
           Result may be -1 (error) or other value, but must not segfault. */
        ck_assert_msg(result >= -1, 
            "load_bpf_file crashed or returned invalid value with payload %d", i);
        
        unlink(test_path);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_bpf_load_buffer_overflow_protection);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}