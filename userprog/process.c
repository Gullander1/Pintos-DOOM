#include "userprog/process.h"

#include "devices/timer.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static thread_func start_process NO_RETURN;
static bool load(char const *file_name, void (**eip)(void), void **esp);
static void dump_stack(void const *esp);

struct parameters_to_start_process {
	char *cmd_line;
};

/**
This is a new struct for the sempahore to work. 
 */
struct load_info {
	struct semaphore load_done;
	const char *cmd_line;
	bool success;
	struct pi *pi;
};

/* Starts a new thread running a user program loaded from
  CMD_LINE. The new thread may be scheduled (and may even exit)
  before process_execute() returns. Returns the new process's
  thread id, or TID_ERROR if the thread cannot be created.
*/
pid_t process_execute(char const *cmd_line)
{
	struct load_info li;

	struct pi *new_pi = malloc(sizeof(struct pi));

	if (new_pi == NULL) {
		return TID_ERROR;
	}

	//struct parameters_to_start_process arguments;
	tid_t thread_id;

	debug(
		"%s#%d: process_execute(\"%s\") ENTERED\n",
		thread_current()->name,
		thread_current()->tid,
		cmd_line
	);

	/* Make a copy of CMD_LINE.
	  Otherwise there's a race between the caller and load(). */
	char *cmd_line_copy = palloc_get_page(0);

	if (cmd_line_copy == NULL) {
		free(new_pi);
		return TID_ERROR;
	}

	strlcpy(cmd_line_copy, cmd_line, PGSIZE);

	// Init all the parameters for a pi
	new_pi->pid = -1;
	new_pi->exit_status = -1;
	new_pi->exited = false;
	new_pi->parent_exited = false;
	sema_init(&new_pi->wait_sema, 0);
	list_push_back(&thread_current()->children, &new_pi->elem);

	li.pi = new_pi;
	li.cmd_line = cmd_line_copy;

	sema_init(&li.load_done, 0);
	li.success = false;

	/* Create a new thread to execute FILE_NAME. */
	thread_id = thread_create(cmd_line_copy, PRI_DEFAULT, start_process, &li);

	if (thread_id == TID_ERROR) {
		list_remove(&new_pi->elem);
		free(new_pi);
		palloc_free_page(cmd_line_copy);
		return TID_ERROR;
	}

	sema_down(&li.load_done);

	palloc_free_page(cmd_line_copy);

	if(!li.success) {
		list_remove(&new_pi->elem);
		return -1;
	}

	new_pi->pid = thread_id;

	debug(
		"%s#%d: process_execute(\"%s\") RETURNS %d\n",
		thread_current()->name,
		thread_current()->tid,
		cmd_line,
		thread_id
	);

	return thread_id;
}

/* Setup the argv and argc parameters (main stack) with some assembly. See
 * `main-stack.S` if you are interested.
 */
void *setup_main_stack_asm(char const *command_line, void *esp);

/* A thread function that loads a user process and starts it
  running. */
static void start_process(void *li_)
{
	struct load_info *li = (struct load_info *) li_; 

	struct intr_frame if_;
	struct thread *t = thread_current();
	bool success;

	t->pi = li->pi;

	debug(
		"%s#%d: start_process(\"%s\") ENTERED\n",
		thread_current()->name,
		thread_current()->tid,
		li->cmd_line
	);

	/* Initialize interrupt frame and load executable. */
	memset(&if_, 0, sizeof if_);
	if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
	if_.cs = SEL_UCSEG;
	if_.eflags = FLAG_IF | FLAG_MBS;

	// Note: load requires the file name only, not the entire cmd_line
	success = load(t->name, &if_.eip, &if_.esp);

	debug(
		"%s#%d: start_process(...): load returned %d\n",
		thread_current()->name,
		thread_current()->tid,
		success
	);

	/* If load success, setup the neccessary */
	if (success) {
		if_.esp = setup_main_stack_asm(li->cmd_line, if_.esp);
	}

	li->success = success;

	if (!success) {
		if (t->pi != NULL) {
			t->pi->exit_status = -1;
		}
	}

	sema_up(&li->load_done);

	debug(
		"%s#%d: start_process(\"%s\") DONE\n",
		thread_current()->name,
		thread_current()->tid,
		li->cmd_line
	);

	/* If load failed, quit. */
	//palloc_free_page((void *) li->cmd_line);
	if (!success) {

		thread_exit();
	}

	/* Start the user process by simulating a return from an
	   interrupt, implemented by intr_exit (in threads/intr-stubs.S). Because
	   intr_exit takes all of its arguments on the stack in the form of a
	   `struct intr_frame', we just point the stack pointer (%esp) to our stack
	   frame and jump to it.
	  */
	asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(&if_) : "memory");
	NOT_REACHED();
}

/* Wait for process `child_id' to die and then return its exit
   status. If it was terminated by the kernel (i.e. killed due to an
   exception), return -1. If `child_id' is invalid or if it was not a child of
   the calling process, or if process_wait() has already been successfully
   called for the given `child_id', return -1 immediately, without waiting.

   This function will be implemented last, after a communication mechanism
   between parent and child is established.
*/
int process_wait(int child_id UNUSED)
{
	struct pi *child_pi = NULL;
	struct thread *cur = thread_current();

	debug("%s#%d: process_wait(%d) ENTERED\n", cur->name, cur->tid, child_id);

	struct list_elem *e = list_begin(&cur->children);

	while (e != list_end(&cur->children)) {
		struct pi *p = list_entry(e, struct pi, elem);

		if (p->pid == child_id) {
			child_pi = p;
			break;
		}

		e = list_next(e);
	}

	if (child_pi == NULL) {
		return -1;
	}

	sema_down(&child_pi->wait_sema);

	int status = child_pi->exit_status;
	list_remove(&child_pi->elem);
	free(child_pi);

	debug("%s#%d: process_wait(%d) RETURNS %d\n", cur->name, cur->tid, child_id, status);

	return status;
}

/* This function is currently never called. As thread_exit does not have an
 * exit status parameter, this could be used to handle that instead. Note
 * however that all cleanup after a process must be done in process_cleanup,
 * and that process_cleanup is already called from thread_exit - do not call
 * cleanup twice!
 */
void process_exit(int status UNUSED) { }

/* Free the current process's resources. */
void process_cleanup(void)
{

	struct thread *cur = thread_current();
	uint32_t *pd;
	
	struct list_elem *e = list_begin(&cur->children);

	while (e != list_end(&cur->children)) {
		struct pi *p = list_entry(e, struct pi, elem);

		if (p->exited) {
			e = list_remove(e);
			free(p);

		} else {
			p->parent_exited = true;
			e = list_next(e);
		}
	}

	//Check if pi is NULL set to status -1, otherwise set the exit status in exit_status.
	// In some cases the pi could be null which would make the kernal panic.
	// In the case that the kernel could not find pi here (because it is null), 
	// it will be set to a default as -1 
	int status = (cur->pi != NULL) ? cur->pi->exit_status : -1;

	
	// Go trough and clean up the file table by closing any instance.
	// Loop trough all 32. 
	for (int i = 2; i < 32; i++) {
		if (cur->file_table[i] != NULL) {
			file_close(cur->file_table[i]);
			cur->file_table[i] = NULL; // Set the value to NULL
		}
	}
	
	debug("%s#%d: process_cleanup() ENTERED\n", cur->name, cur->tid);

	/* Later tests DEPEND on this output to work correct. It is important to do
	 * this printf BEFORE you tell the parent process that you exit. (Since the
	 * parent may be the main() function, that may sometimes poweroff as soon
	 * as process_wait() returns, possibly before the printf is completed.)
	 */
	printf("%s: exit(%d)\n", cur->name, status);

	if (cur->pi != NULL) {
		cur->pi->exited = true;
		bool should_free = cur->pi->parent_exited;
		sema_up(&cur->pi->wait_sema);

		if (should_free) {
			free(cur->pi);
		}
	}

	/* Destroy the current process's page directory and switch back
	  to the kernel-only page directory. */
	pd = cur->pagedir;
	if (pd != NULL) {
		/* Correct ordering here is crucial.  We must set
		  cur->pagedir to NULL before switching page directories,
		  so that a timer interrupt can't switch back to the
		  process page directory.  We must activate the base page
		  directory before destroying the process's page
		  directory, or our active page directory will be one
		  that's been freed (and cleared). */
		cur->pagedir = NULL;
		pagedir_activate(NULL);
		pagedir_destroy(pd);
	}

	debug("%s#%d: process_cleanup() DONE with status %d\n", cur->name, cur->tid, status);
}

/* Sets up the CPU for running user code in the current
  thread.
  This function is called on every context switch. */
void process_activate(void)
{
	struct thread *t = thread_current();

	/* Activate thread's page tables. */
	pagedir_activate(t->pagedir);

	/* Set thread's kernel stack for use in processing
	  interrupts. */
	tss_update();
}

/* We load ELF binaries.  The following definitions are taken
  from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
  This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
	unsigned char e_ident[16];
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off e_phoff;
	Elf32_Off e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
  There are e_phnum of these, starting at file offset e_phoff
  (see [ELF1] 1-6). */
struct Elf32_Phdr {
	Elf32_Word p_type;
	Elf32_Off p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0          /* Ignore. */
#define PT_LOAD    1          /* Loadable segment. */
#define PT_DYNAMIC 2          /* Dynamic linking info. */
#define PT_INTERP  3          /* Name of dynamic loader. */
#define PT_NOTE    4          /* Auxiliary info. */
#define PT_SHLIB   5          /* Reserved. */
#define PT_PHDR    6          /* Program header table. */
#define PT_STACK   0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(
	struct file *file,
	off_t ofs,
	uint8_t *upage,
	uint32_t read_bytes,
	uint32_t zero_bytes,
	bool writable
);

/* Loads an ELF executable from FILE_NAME into the current thread.
  Stores the executable's entry point into *EIP
  and its initial stack pointer into *ESP.
  Returns true if successful, false otherwise. */
bool load(char const *file_name, void (**eip)(void), void **esp)
{
	struct thread *t = thread_current();
	struct Elf32_Ehdr ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pagedir = pagedir_create();
	if (t->pagedir == NULL) {
		goto done;
	}
	process_activate();

	/* Open executable file. */
	file = filesys_open(file_name);
	if (file == NULL) {
		printf("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. */
	if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7)
	    || ehdr.e_type != 2 || ehdr.e_machine != 3 || ehdr.e_version != 1
	    || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024) {
		printf("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Elf32_Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length(file)) {
			goto done;
		}
		file_seek(file, file_ofs);

		if (file_read(file, &phdr, sizeof phdr) != sizeof phdr) {
			goto done;
		}
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment(&phdr, file)) {
					bool writable = true;
					uint32_t file_page = phdr.p_offset & ~PGMASK;
					uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint32_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						  Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
					}
					else {
						/* Entirely zero.
						  Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment(file, file_page, (void *) mem_page, read_bytes, zero_bytes, writable)) {
						goto done;
					}
				}
				else {
					goto done;
				}
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack(esp)) {
		goto done;
	}

	/* Start address. */
	*eip = (void (*)(void)) ehdr.e_entry;

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	file_close(file);
	return success;
}

/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
  FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Elf32_Phdr *phdr, struct file *file)
{
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) {
		return false;
	}

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (Elf32_Off) file_length(file)) {
		return false;
	}

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz) {
		return false;
	}

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0) {
		return false;
	}

	/* The virtual memory region must both start and end within the
	  user address space range. */
	if (!is_user_vaddr((void *) phdr->p_vaddr)) {
		return false;
	}
	if (!is_user_vaddr((void *) (phdr->p_vaddr + phdr->p_memsz))) {
		return false;
	}

	/* The region cannot "wrap around" across the kernel virtual
	  address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr) {
		return false;
	}

	/* Disallow mapping page 0.
	  Not only is it a bad idea to map page 0, but if we allowed
	  it then user code that passed a null pointer to system calls
	  could quite likely panic the kernel by way of null pointer
	  assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE) {
		return false;
	}

	/* It's okay. */
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
  UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
  memory are initialized, as follows:

      - READ_BYTES bytes at UPAGE must be read from FILE
       starting at offset OFS.

      - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

  The pages initialized by this function must be writable by the
  user process if WRITABLE is true, read-only otherwise.

  Return true if successful, false if a memory allocation error
  or disk read error occurs. */
static bool load_segment(
	struct file *file,
	off_t ofs,
	uint8_t *upage,
	uint32_t read_bytes,
	uint32_t zero_bytes,
	bool writable
)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Calculate how to fill this page.
		  We will read PAGE_READ_BYTES bytes from FILE
		  and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page(PAL_USER);
		if (kpage == NULL) {
			return false;
		}

		/* Load this page. */
		if (file_read(file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page(kpage);
			return false;
		}
		memset(kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page(upage, kpage, writable)) {
			palloc_free_page(kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
  user virtual memory. */
static bool setup_stack(void **esp)
{
    bool success = true;
    
    size_t stack_pages = 256; 

    for (size_t i = 1; i <= stack_pages; i++) {
        uint8_t *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
        if (kpage == NULL) {
            return false;
        }

        if (!install_page(((uint8_t *) PHYS_BASE) - (i * PGSIZE), kpage, true)) {
            palloc_free_page(kpage);
            return false;
        }
    }

    *esp = PHYS_BASE;
    return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
  virtual address KPAGE to the page table.
  If WRITABLE is true, the user process may modify the page;
  otherwise, it is read-only.
  UPAGE must not already be mapped.
  KPAGE should probably be a page obtained from the user pool
  with palloc_get_page().
  Returns true on success, false if UPAGE is already mapped or
  if memory allocation fails. */
static bool install_page(void *upage, void *kpage, bool writable)
{
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	  address, then map our page there. */
	return (
		pagedir_get_page(t->pagedir, upage) == NULL
		&& pagedir_set_page(t->pagedir, upage, kpage, writable)
	);
}

// Don't raise a warning about unused function.
// We know that dump_stack might not be called, this is fine.

#pragma GCC diagnostic ignored "-Wunused-function"
/* With the given stack pointer, will try and output the stack to STDOUT. */
static void dump_stack(void const *esp)
{
	printf("*esp is %p\nstack contents:\n", esp);
	hex_dump((int) esp, esp, PHYS_BASE - esp + 16, true);
	/* The same information, only more verbose: */
	/* It prints every byte as if it was a char and every 32-bit aligned
	  data as if it was a pointer. */
	void *ptr_save = PHYS_BASE;
	int i = -15;
	while (ptr_save - i >= esp) {
		char *whats_there = (char *) (ptr_save - i);
		// show the address ...
		printf("%x\t", (uint32_t) whats_there);
		// ... printable byte content ...
		if (*whats_there >= 32 && *whats_there < 127) {
			printf("%c\t", *whats_there);
		}
		else {
			printf(" \t");
		}
		// ... and 32-bit aligned content
		if (i % 4 == 0) {
			uint32_t *wt_uint32 = (uint32_t *) (ptr_save - i);
			printf("%x\t", *wt_uint32);
			printf("\n-------");
			if (i != 0) {
				printf("------------------------------------------------");
			}
			else {
				printf(" the border between KERNEL SPACE and USER SPACE ");
			}
			printf("-------");
		}
		printf("\n");
		i++;
	}
}
#pragma GCC diagnostic pop
