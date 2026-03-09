#include "userprog/syscall.h"

#include "threads/interrupt.h"
#include "threads/thread.h"

#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>

#include "devices/shutdown.h"
#include "lib/kernel/stdio.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "devices/timer.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/gui.h"

static void validate_ptr(const void *vaddr);
static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}
/* This array defines the number of arguments each syscall expects.
   For example, if you want to find out the number of arguments for
   the read system call you can write:

   int sys_read_arg_count = ARGC[ SYS_READ ];

   All system calls have a name such as SYS_READ defined as an enum
   type, see `lib/syscall-nr.h'. Use them instead of numbers.

   The code below uses explicit indices to make it easier to verify
   that the correct value is used for the correct syscall. The syntax
   [x] = y used below means that index x in the array will be given
   the value y. That is ARGC[x] will equal y.
 */
int const ARGC[] = {
	/* basic calls */
	[SYS_HALT] = 0,
	[SYS_EXIT] = 1,
	[SYS_EXEC] = 1,
	[SYS_WAIT] = 1,
	[SYS_CREATE] = 2,
	[SYS_REMOVE] = 1,
	[SYS_OPEN] = 1,
	[SYS_FILESIZE] = 1,
	[SYS_READ] = 3,
	[SYS_WRITE] = 3,
	[SYS_SEEK] = 2,
	[SYS_TELL] = 1,
	[SYS_CLOSE] = 1,
	[SYS_MMAP] = 2,
	[SYS_MUNMAP] = 1,
	[SYS_CHDIR] = 1,
	[SYS_MKDIR] = 1,
	[SYS_READDIR] = 2,
	[SYS_ISDIR] = 1,
	[SYS_INUMBER] = 1,
	[SYS_SLEEP] = 1,
	[SYS_DRAW_FRAME] = 3,
	[SYS_TIME] = 0,
	[SYS_GETC] = 0,

	/* Explicitly set NUMBER_OF_CALLS to make the array large enough and catch errors */
	[SYS_NUMBER_OF_CALLS] = 0
};

// Function to validate pointer is valid
static void validate_ptr(const void *vaddr)
{
	// Use is_user_vddr
	if (vaddr == NULL || !is_user_vaddr(vaddr) || pagedir_get_page(thread_current()->pagedir, vaddr) == NULL) {

		if (thread_current()->pi != NULL) {
			thread_current()->pi->exit_status = -1;
		}

		thread_exit();
	}
}

// Function to validate strings
static void validate_str(const char *c) 
{
	if (c == NULL) {
		thread_exit();
	}

	validate_ptr(c);

	while (true) {

		// Check if it has reached the end
		if (*c == '\0') {
			break;
		}

		c++;

		if (((uintptr_t)c % PGSIZE) == 0) {
			validate_ptr(c);
		}
	}

}

// Validate the buffer
static void validate_bfr(const void* buffer, unsigned size) {
    if (size == 0) return;
    const char *ptr = (const char *)buffer;

    for (unsigned i = 0; i < size; i += PGSIZE) {
        if (ptr + i == NULL || !is_user_vaddr(ptr + i)) {
            thread_exit();
        }
        
        volatile char dummy = *(ptr + i); 
        (void)dummy;
    }
}

static void syscall_handler(struct intr_frame *f)
{
	validate_bfr(f->esp, sizeof(int32_t));

	int32_t *esp = (int32_t *) f->esp;
	int syscall_nr = esp[0];

	//printf("SYSCALL CALL: %d\n", syscall_nr);

	if( syscall_nr < 0 || syscall_nr >= SYS_NUMBER_OF_CALLS) {
		thread_exit();
	}

	for (int i = 0; i <= ARGC[syscall_nr]; i++) {
		validate_bfr(&esp[i], sizeof(int32_t));
	}

	switch (syscall_nr) {

		/* This function will make the current process sleep for a time. */
		case SYS_SLEEP:
		{	
			// The time to sleep for.
			int ms = esp[1];
			if (ms > 0) {
				// Call the already given function.
				timer_msleep(ms);
			}
			break;
		}

		/* This function will shut the whole system down. */
		case SYS_HALT:
		{
			// Halt the process with the given function
			shutdown_power_off();
			break;
		}

		/* This function will terminate the userprogram, and pass an exitstatus to the kernel */
		case SYS_EXIT:
		{
			// Exit the process
			if(thread_current()->pi != NULL) {
				thread_current()->pi->exit_status = esp[1];
			}

			// Exit the thread.
			thread_exit();
			break;
		}

		/* This function will write out size bytes from buffer into an already open file thats found in fd */
		case SYS_WRITE:
        {
            int fd = esp[1];
			const void *buffer = (void *) esp[2];
			unsigned size = (unsigned) esp[3];

			validate_bfr(buffer, size);

				if (fd == 1) {
					
					putbuf(buffer, size); 
					
					struct thread *cur = thread_current();
					//if (cur->my_window != NULL) {
						//wm_write_to_window(cur->my_window, buffer, size);
					//}
					f->eax = size;

				} else if (fd >= 2 && fd < 32) {

                struct file *fp = thread_current()->file_table[fd];
                if (fp != NULL) {
                    f->eax = file_write(fp, buffer, size);
                } else {
                    f->eax = -1;
                }

            } else {
                f->eax = -1;
            }
            break;
        }

		/* This function will read size bytes from an already open file corresponding to fd into buffer */
		case SYS_READ:
        {
            int fd = (int) esp[1];
            void *buffer = (void *) esp[2];
            unsigned size = (unsigned) esp[3];

            // 2. Blockera orimliga storlekar direkt
            if (size > 16 * 1024 * 1024) { 
                f->eax = -1;
                break; 
            }

            validate_bfr(buffer, size);

            if (fd == 0) { // STDIN
                unsigned i;
                uint8_t *local_buffer = (uint8_t *) buffer;
                for (i = 0; i < size; i++) {
                    local_buffer[i] = input_getc();
                    if (local_buffer[i] == '\r') local_buffer[i] = '\n';
                }
                f->eax = size;
            } 
            else if (fd >= 2 && fd < 32) {
                struct file *fp = thread_current()->file_table[fd];
                if (fp != NULL) {
                    f->eax = file_read(fp, buffer, size);
                } else {
                    f->eax = -1;
                }
            } else {
                f->eax = -1;
            }
            break;
        }	
	
		/* This function will creates a new file that will be called file. Returns true/false. */
		case SYS_CREATE:
		{
			char *name = (char *) esp[1];

			validate_str(name);

			unsigned size = (unsigned) esp[2];

			// Create the file with the given function
			f->eax = filesys_create(name, size);
			break;
		}

		/* This function will open the file called file. Will return a fd (file descriptor). */
		case SYS_OPEN:
        {
            const char *name = (const char *) esp[1];
            validate_str(name);

            struct file *fp = filesys_open(name);
            
            if (fp == NULL) {
                f->eax = -1;
            } else {

                struct thread *t = thread_current();
                int fd = -1;
                
                for (int i = 2; i < 32; i++) {
                    if (t->file_table[i] == NULL) {
                        t->file_table[i] = fp;
                        fd = i;
                        break;
                    }
                }

                f->eax = fd;
                
                if (fd == -1) {
                    file_close(fp);
                }
            }
            break;
        }

		/* Close the file corresponding to the given fd. Freeing up memory in the kernel. */
		case SYS_CLOSE:
		{
			int fd = esp[1];
			if (fd >= 2 && fd < 32) {
				struct thread *t = thread_current();
				if (t->file_table[fd] != NULL) {
					file_close(t->file_table[fd]);
					t->file_table[fd] = NULL;
				}
			}
			break;
		}
		
		/* Removes the file with the name file_name. Returns true/false. */
		case SYS_REMOVE:
		{	
			// Get the argument on the stack (const *file in this case)
			const char *name = (const char *) esp[1];

			validate_str(name);

			f->eax = filesys_remove(name);
			break;
		}

		/* Sets the current position of the file corresponding to fd to position. */
		case SYS_SEEK:
        {
            int fd = esp[1];
            unsigned position = (unsigned) esp[2];
            
            if(fd >= 2 && fd < 32) {
                struct file *fp = thread_current()->file_table[fd];
                if(fp != NULL) {
                    file_seek(fp, position);
                }
            }
            break;
        }

		/* Returns the current position in the file corresponding to fd */
		case SYS_TELL:
		{
			int fd = esp[1];

			if(fd >= 2 && fd < 32) {
				struct file *fp = thread_current()->file_table[fd];

				if(fp != NULL) {
					f->eax = file_tell(fp);
				} else {
					f->eax = -1;
				}
			} else {
				f->eax = -1;
			}

			break;
		}

		/* Returns the length of the file corresponding to fd */
		case SYS_FILESIZE:
		{
			int fd = esp[1];

			if(fd >= 2 && fd < 32) {
				struct file *fp = thread_current()->file_table[fd];

				if (fp != NULL) {
					f->eax = file_length(fp);
				} else {
					f->eax = -1;
				}

			} else {
				f->eax = -1;
			}

			break;
		}

		case SYS_EXEC:
		{
			const char* cmd_line = (const char *) esp[1];

			validate_str(cmd_line);

			f->eax = process_execute(cmd_line);
			break;
		}

		case SYS_WAIT:
		{
			int pid = esp[1];
			f->eax = process_wait(pid);
			break;
		}

		case SYS_DRAW_FRAME:
		{
			uint32_t *buffer = (uint32_t *)esp[1];
			int width = esp[2];
			int height = esp[3];

			validate_bfr(buffer, width * height * 4);

			struct thread *cur = thread_current();
			if (cur->my_window == NULL && !list_empty(&window_list)) {
				cur->my_window = list_entry(list_begin(&window_list), struct window, elem);
			}
			
			if (cur->my_window == NULL) {
				if (!list_empty(&window_list)) {
					struct list_elem *e = list_begin(&window_list);
					cur->my_window = list_entry(e, struct window, elem);
				}
			}

			if (cur->my_window != NULL) {
				wm_draw_pixel_block(cur->my_window, buffer, width, height);
			}
			break;
		}

		case SYS_TIME:
        {
            f->eax = (uint32_t) timer_ticks();
            break;
        }

		case SYS_GETC:
		{
			if (input_empty()) {
				f->eax = -1; 
			} else {
				f->eax = input_getc();
			}
			break;
		}

		default:
			printf("WARNING: Unimplemented syscall %d called!\n", syscall_nr);
            
            f->eax = -1; 
            break;
	}
}
