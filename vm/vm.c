/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
 vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct hash *spt = &thread_current ()->spt; 
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */

		// 1.가상 메모리 유형에 따라 uninit 페이지 초기화 
		struct page *new_page = calloc(sizeof(struct page),1); 
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			uninit_new(new_page,upage,init,type,aux,anon_initializer);
			break;
		case VM_FILE:
			uninit_new(new_page,upage,init,type,aux,file_backed_initializer);
		}
		
		// 2.보조 페이지 테이블에 삽입
		if(!spt_insert_page(spt,new_page))
		{
			printf("[FAIL] vm_alloc_page_with_initializer spt_insert_page\n");
			goto err;
		}
	}
	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct hash *spt UNUSED, void *va UNUSED) {
	struct page page;
	struct hash_elem *e;
	page.va = pg_round_down(va);
	e = hash_find(spt,&page.hash_elem);
	if (e == NULL)
		return NULL;
	return hash_entry(e,struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct hash *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	hash_insert(spt,&page->hash_elem);
	succ= true;
	return succ;
}

void
spt_remove_page (struct hash *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
 
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = calloc(sizeof(struct frame),1); 
	frame->kva = palloc_get_page(PAL_ZERO | PAL_USER);
	frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	if(frame->kva == NULL){
		free(frame);
		printf("[FAIL] vm_get_frame kva palloc fail\n"); 
		PANIC("todo");
	}
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	vm_alloc_page(VM_ANON | IS_STACK, addr,true);
	vm_claim_page(addr);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct hash *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt,addr);
	// printf("[START] vm_try_handle_fault\n");
	if(is_kernel_vaddr(addr)&&user)
		return false;
	
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// printf("[END] vm_try_handle_fault\n");
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt,va);
	if(page == NULL)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	// printf("[START]vm_do_claim_page\n");
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current();
	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(!pml4_set_page(t->pml4,page->va,frame->kva,true))
	{
		printf("[FAIL]vm_do_claim_page pml4_set_page fail\n");
	}
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct hash *spt UNUSED) {
	// printf("[START] supplemental_page_table_init \n");

	if(!hash_init(spt,page_hash,spt_less_func,NULL))
	{
		printf("[FAIL] supplemental_page_table_init. hash init\n");
	}
	// printf("[END] supplemental_page_table_init \n");
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct hash *dst UNUSED,
		struct hash *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct hash *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
