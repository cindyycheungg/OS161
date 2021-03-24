/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include "opt-A2.h"
#include "opt-A3.h"

#if OPT_A2

#include <copyinout.h>

#endif

#if OPT_A3

#include <synch.h>

#endif


/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3

struct lock * coreMapLock;
bool coreMapInitialized = false; 
paddr_t coreMapStart; 
paddr_t firstFrame; 
int totalEntries = 0; 

#endif

#if OPT_A3
	void vm_bootstrap(void){
		coreMapLock = lock_create("coreMapLock");
		//start to create the core map 

		// 1. call ram_getsize to get thre remaining physical memeory in the system 
		paddr_t high, low; 
		
		lock_acquire(coreMapLock);
			ram_getsize(&low, &high);
			coreMapStart = low; 
			//2. Partition remaining mem into fixed size frames 
			paddr_t remainingMem = high - low; 
			// calculate size of coremap - 1 entry per frame 
			int coreMapSize = sizeof(int) * remainingMem/PAGE_SIZE; 

			totalEntries = remainingMem/PAGE_SIZE; 

			firstFrame = ROUNDUP(low + coreMapSize, PAGE_SIZE); 

			//store coremap
			for(int i = 0; i < coreMapSize; i++){
				*(int *) PADDR_TO_KVADDR(low) = 0; 
				low += sizeof(int); 
			}

			//pad with 0's of coremap size is less than framesize 
			while(low < firstFrame){
				*(int *) PADDR_TO_KVADDR(low) = 0; 
				low += sizeof(int); 
			}
			coreMapInitialized = true; 
		lock_release(coreMapLock); 
	}
#else
	void vm_bootstrap(void){
		/* Do nothing. */
	}
#endif

static paddr_t getppages(unsigned long npages){
	paddr_t addr;

	#if OPT_A3
		if(!coreMapInitialized){
			spinlock_acquire(&stealmem_lock);
				addr = ram_stealmem(npages);
			spinlock_release(&stealmem_lock);
		}
		else{ //use the coremap 
			lock_acquire(coreMapLock); 
				//iterate through coremap and look for consecutive npages of 0's 
				unsigned long zeroCount = 0; 
				paddr_t chainStart; 
				bool setAddress = false; 
				for(int i = 0; i < totalEntries; i++){
					if(zeroCount == npages){
						//go through entries and mark as 1, 2, 3...
						for(unsigned int j = 1; j <= npages; j++){
							*(int *) PADDR_TO_KVADDR(chainStart) = j; 
							chainStart += sizeof(int); 
						}
						addr = firstFrame + (PAGE_SIZE * (i - npages));
						setAddress = true; 
						break; 
					}
					else{
						if(*(int *)PADDR_TO_KVADDR(coreMapStart + (i * sizeof(int))) == 0){
							if(zeroCount == 0){
								chainStart = coreMapStart + (i * sizeof(int)); 
							}
							zeroCount += 1; 
						}
						else{
							zeroCount = 0; 
						}
					}
				}
				if(setAddress == false){
					addr = 0;
				}
			lock_release(coreMapLock);
		}
		return addr; 
	#else
		spinlock_acquire(&stealmem_lock);
			addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
		return addr;
	#endif
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t alloc_kpages(int npages){
	paddr_t pa;
	pa = getppages(npages);
	if (pa == 0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr){
	#if OPT_A3
		lock_acquire(coreMapLock);
			vaddr_t diff = addr - PADDR_TO_KVADDR(firstFrame); 
			int index = diff / PAGE_SIZE; 

			*(int *)PADDR_TO_KVADDR(coreMapStart + (index*sizeof(int))) = 0; 
			index += 1; 

			while(*(int *)PADDR_TO_KVADDR(coreMapStart + (index*sizeof(int))) > 1){
				*(int *)PADDR_TO_KVADDR(coreMapStart + (index*sizeof(int))) = 0; 
				index += 1;
			}
		lock_release(coreMapLock); 
	#else
		(void)addr; 
	#endif
}

void vm_tlbshootdown_all(void){
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void vm_tlbshootdown(const struct tlbshootdown *ts){
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int vm_fault(int faulttype, vaddr_t faultaddress){
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
			#if OPT_A3 
				case VM_FAULT_READONLY:
					// trying to write to read only memory - need to return error code 
					// return bad memory reference
					return EFAULT; 
			#else
				case VM_FAULT_READONLY:
					/* We always create pages read-write, so we can't get this */
					panic("dumbvm: got VM_FAULT_READONLY\n");
			#endif 
			
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	#if OPT_A3
		KASSERT(as->as_vbase1 != 0);
		KASSERT(as->as_npages1 != 0);
		KASSERT(as->pbase1_pt != NULL);
		KASSERT(as->as_vbase2 != 0);
		KASSERT(as->pbase2_pt != NULL);
		KASSERT(as->as_npages2 != 0);
		KASSERT(as->stackpbase_pt != NULL);
		KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
		paddr_t assertAddress; 
		for(unsigned int i = 0; i < as->as_npages1; i++){
			assertAddress = (paddr_t)array_get(as->pbase1_pt, i); 
			KASSERT((assertAddress & PAGE_FRAME) == assertAddress);
		}
		KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
		for(unsigned int i = 0; i < as->as_npages2; i++){
			assertAddress = (paddr_t)array_get(as->pbase2_pt, i); 
			KASSERT((assertAddress & PAGE_FRAME) == assertAddress);
		}
		for(int i = 0; i < DUMBVM_STACKPAGES; i++){
			assertAddress = (paddr_t)array_get(as->stackpbase_pt, i); 
			KASSERT((assertAddress & PAGE_FRAME) == assertAddress);
		}
	
	#else
		KASSERT(as->as_vbase1 != 0);
		KASSERT(as->as_pbase1 != 0);
		KASSERT(as->as_npages1 != 0);
		KASSERT(as->as_vbase2 != 0);
		KASSERT(as->as_pbase2 != 0);
		KASSERT(as->as_npages2 != 0);
		KASSERT(as->as_stackpbase != 0);
		KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
		KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
		KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
		KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
		KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
	#endif 

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	#if OPT_A3
		bool codeSeg = false; 
	#endif

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		//we are in the code segment 
		#if OPT_A3
			codeSeg = true; 
			paddr = (faultaddress - vbase1) + (paddr_t)array_get(as->pbase1_pt, 0);
		#else
			paddr = (faultaddress - vbase1) + as->as_pbase1;
		#endif
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		#if OPT_A3
			paddr = (faultaddress - vbase2) + (paddr_t)array_get(as->pbase2_pt, 0);
		#else
			paddr = (faultaddress - vbase2) + as->as_pbase2;
		#endif
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		#if OPT_A3
			paddr = (faultaddress - stackbase) + (paddr_t)array_get(as->stackpbase_pt, 0);
		#else
			paddr = (faultaddress - stackbase) + as->as_stackpbase;
		#endif 
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		
		#if OPT_A3
			if(codeSeg && as->as_loaded){
				//turn elo dirty off 
				elo &= ~TLBLO_DIRTY; 
			}
		#endif

		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	#if OPT_A3
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		tlb_random(ehi, elo); 
		splx(spl);
		return 0; 
	
	#else
		kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
		splx(spl);
		return EFAULT;
	#endif /* OPT A3 */
	
}

struct addrspace * as_create(void){
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}
	
	#if OPT_A3
		as->as_loaded = false; 
		as->as_vbase1 = 0;
		as->pbase1_pt = array_create();
		as->as_npages1 = 0;
		as->as_vbase2 = 0;
		as->pbase2_pt = array_create();
		as->as_npages2 = 0;
		as->stackpbase_pt = array_create();
		as->asLock = lock_create("addrspace Lock"); 
	#else 
		as->as_vbase1 = 0;
		as->as_pbase1 = 0;
		as->as_npages1 = 0;
		as->as_vbase2 = 0;
		as->as_pbase2 = 0;
		as->as_npages2 = 0;
		as->as_stackpbase = 0;
	#endif

	return as;
}

void as_destroy(struct addrspace *as){

	#if OPT_A3
		paddr_t freeAddr; 
		for(unsigned int i = 0; i < as->as_npages1; i++){
			freeAddr = (paddr_t)array_get(as->pbase1_pt, i); 	
			free_kpages((vaddr_t)PADDR_TO_KVADDR(freeAddr));
		}
		for(unsigned int i = 0; i < as->as_npages2; i++){
			freeAddr = (paddr_t)array_get(as->pbase2_pt, i); 	
			free_kpages((vaddr_t)PADDR_TO_KVADDR(freeAddr));
		}
		for(unsigned int i = 0; i < DUMBVM_STACKPAGES; i++){
			freeAddr = (paddr_t)array_get(as->stackpbase_pt, i); 	
			free_kpages((vaddr_t)PADDR_TO_KVADDR(freeAddr));
		}
		kfree(as->pbase1_pt); 
		kfree(as->pbase2_pt); 
		kfree(as->stackpbase_pt); 
	
	#endif 
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz, int readable, int writeable, int executable){
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		//set size of page table 
		#if OPT_A3
			lock_acquire(as->asLock); 
				int pbase1Size = array_setsize(as->pbase1_pt, as->as_npages1); 
				if(pbase1Size != 0){
					return EFAULT; 
				}
			lock_release(as->asLock); 
		#endif 
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		#if OPT_A3
			lock_acquire(as->asLock); 
				int pbase2Size = array_setsize(as->pbase2_pt, as->as_npages2); 
				if(pbase2Size != 0){
					return EFAULT; 
				}
			lock_release(as->asLock); 
		#endif 
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

#if OPT_A3
	int as_prepare_load(struct addrspace *as){
		KASSERT(array_num(as->pbase1_pt) == as->as_npages1);
		KASSERT(array_num(as->pbase2_pt) == as->as_npages2);

		//set up stackpbase 
		lock_acquire(as->asLock); 
			int stackpbaseSize = array_setsize(as->stackpbase_pt, DUMBVM_STACKPAGES); 
			if(stackpbaseSize != 0){
				return EFAULT; 
			}
		lock_release(as->asLock); 

		KASSERT(array_num(as->stackpbase_pt) == DUMBVM_STACKPAGES);

		lock_acquire(as->asLock); 
			for(unsigned int i = 0; i < as->as_npages1; i++){
				array_set(as->pbase1_pt, i, (void *)getppages(1));
				if (array_get(as->pbase1_pt, i) == 0) {
					return ENOMEM;
				}
			}
		lock_release(as->asLock); 

		lock_acquire(as->asLock); 
			for(unsigned int i = 0; i < as->as_npages2; i++){
				array_set(as->pbase2_pt, i, (void *)getppages(1));
				if (array_get(as->pbase2_pt, i) == 0) {
					return ENOMEM;
				}
			}
		lock_release(as->asLock); 

		lock_acquire(as->asLock); 
			for(int i = 0; i < DUMBVM_STACKPAGES; i++){
				array_set(as->stackpbase_pt, i, (void *)getppages(1));
				if (array_get(as->stackpbase_pt, i) == 0) {
					return ENOMEM;
				}
			}
		lock_release(as->asLock); 
		
		lock_acquire(as->asLock); 
			for(unsigned int i = 0; i < as->as_npages1; i++){
				as_zero_region((paddr_t)array_get(as->pbase1_pt, i), 1);
			}
		lock_release(as->asLock); 

		lock_acquire(as->asLock); 
			for(unsigned int i = 0; i < as->as_npages2; i++){
				as_zero_region((paddr_t)array_get(as->pbase2_pt, i), 1);
			}
		lock_release(as->asLock); 

		lock_acquire(as->asLock); 
			for(int i = 0; i < DUMBVM_STACKPAGES; i++){
				as_zero_region((paddr_t)array_get(as->stackpbase_pt, i), 1);
			}
		lock_release(as->asLock); 

		return 0;
	}
#else
	int as_prepare_load(struct addrspace *as){
		KASSERT(as->as_pbase1 == 0);
		KASSERT(as->as_pbase2 == 0);
		KASSERT(as->as_stackpbase == 0);

		as->as_pbase1 = getppages(as->as_npages1);
		if (as->as_pbase1 == 0) {
			return ENOMEM;
		}

		as->as_pbase2 = getppages(as->as_npages2);
		if (as->as_pbase2 == 0) {
			return ENOMEM;
		}

		as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
		if (as->as_stackpbase == 0) {
			return ENOMEM;
		}
		
		as_zero_region(as->as_pbase1, as->as_npages1);
		as_zero_region(as->as_pbase2, as->as_npages2);
		as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

		return 0;
	}
#endif 

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr){
	
	#if OPT_A3
		KASSERT(as->stackpbase_pt != NULL);
	#else
		KASSERT(as->as_stackpbase != 0);
	#endif 

	*stackptr = USERSTACK;
	return 0;
}

#if OPT_A2

int as_define_stack_new(struct addrspace *as, vaddr_t *stackptr, char** kernelArgs, int totalArgs, char* kernelProgram, vaddr_t *usrSpaceAddrArgv){
	
	#if OPT_A3
		KASSERT(as->stackpbase_pt != NULL);
	#else
		KASSERT(as->as_stackpbase != 0);
	#endif 

	*stackptr = USERSTACK;

	int totalArgSize = 0;
	//find total size of all arguments not including NULL at the end 
	for(int i = 0; i < totalArgs; i++){
		totalArgSize += strlen(kernelArgs[i]) + 1; 
	}
	//include program name 
	totalArgSize += strlen(kernelProgram) + 1; 
	//round up the arguments to 8 
	size_t roundedTotalArgSize = ROUNDUP(totalArgSize, 8); 
	//starting address of the first char of what you wanna push on to userstack 
	vaddr_t argStartingAddr = *stackptr - (vaddr_t)roundedTotalArgSize; 

	//starting address for copying addresses
	vaddr_t addressStartingAddr = *stackptr - (vaddr_t)roundedTotalArgSize; 
	
	//making array of vaddr_t 
	vaddr_t *stackArgAddr = kmalloc((totalArgs + 1) * sizeof(vaddr_t)); 

	//find size of program name 
  size_t programNameSize = sizeof(char) * (strlen((const char *)kernelProgram) + 1); 
	int copyProgOut = copyoutstr((const char *)kernelProgram, (userptr_t)argStartingAddr, programNameSize, NULL); 
	if(copyProgOut != 0){
		return ENOMEM; 
	}
	//add prog starting address into vaddr_t array 
	stackArgAddr[0] = argStartingAddr; 
	//increase argStartAddr cause you're going down  
	argStartingAddr += programNameSize; 
	
	//copy arguments into userstack 
	for(int i = 0; i < totalArgs; i++){
		//find size of argument name
		size_t argNameSize = sizeof(char) * (strlen((const char *)kernelArgs[i]) + 1); 
		int copyArgOut = copyoutstr((const char *)kernelArgs[i], (userptr_t)argStartingAddr, argNameSize, NULL); 
		if(copyArgOut != 0){
			return ENOMEM; 
		}
		//add prog starting address into vaddr_t array 
		stackArgAddr[i + 1] = argStartingAddr; 
		//increase argStartAddr cause you're going down  
		argStartingAddr += argNameSize; 
	}

	//pad with 0s if needed 
	int zerosToPad = roundedTotalArgSize - totalArgSize; 
	if(zerosToPad != 0){
		int zeroSize = 1;
		int zeroPointer = 0;  
		for(int i = 0; i < zerosToPad; i++){
			int copyZeros = copyout((void *)&zeroPointer, (userptr_t)argStartingAddr, (size_t)zeroSize); 
			argStartingAddr += zeroSize; 
			if(copyZeros != 0){
				return ENOMEM; 
			}
		}	
	}

	//copy the addresses to the user stack 
	// want to put program name at address mod 8
	int totalAddressSize = (totalArgs + 2) * sizeof(vaddr_t); 
	size_t roundedTotalAddressSize = ROUNDUP(totalAddressSize, 8); 
	vaddr_t addrStart = addressStartingAddr - roundedTotalAddressSize; 

	//set the userspace address of argv to return b/c we need it for enter_new_process
	*usrSpaceAddrArgv = addrStart;
	*stackptr = addrStart;

	size_t vaddrSize = sizeof(vaddr_t); 
	
	//copy address of program name in 
	vaddr_t progAddress = stackArgAddr[0]; 
	int copyProgNameAddress = copyout((void*) &progAddress, (userptr_t)addrStart, vaddrSize); 
	if(copyProgNameAddress != 0){
		return ENOMEM; 
	}
	//increase addrStart 
	addrStart += vaddrSize; 

	for(int i = 0; i < totalArgs; i++){
		vaddr_t argAddress = stackArgAddr[i + 1]; 
		int copyArgAddress = copyout((void*) &argAddress, (userptr_t)addrStart, vaddrSize); 
		if(copyArgAddress != 0){
			return ENOMEM; 
		}
		addrStart += vaddrSize; 
	}

	//put null at the last addrSize 
	int nullPtr = 0; 
	int copyNull = copyout((void *)&nullPtr, (userptr_t)addrStart, vaddrSize); 
	if(copyNull != 0){
		return ENOMEM; 
	}
	
	kfree(stackArgAddr);

	return 0;
}

#endif /* OPT_A2 */


int as_copy(struct addrspace *old, struct addrspace **ret){
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	#if OPT_A3
		int new_pbase1Size = array_setsize(new->pbase1_pt, new->as_npages1); 
		if(new_pbase1Size != 0){
			return EFAULT; 
		}

		int new_pbase2Size = array_setsize(new->pbase2_pt, new->as_npages2); 
		if(new_pbase2Size != 0){
			return EFAULT; 
		}
	#endif 

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	#if OPT_A3
		KASSERT(new->pbase1_pt != NULL);
		KASSERT(new->pbase2_pt != NULL);
		KASSERT(new->stackpbase_pt != NULL);

		paddr_t oldAddr; 
		paddr_t newAddr; 
		for(unsigned int i = 0; i < new->as_npages1; i++){
			oldAddr = (paddr_t)array_get(old->pbase1_pt, i); 
			newAddr = (paddr_t)array_get(new->pbase1_pt, i); 
			memmove((void *)PADDR_TO_KVADDR(newAddr), (const void *)PADDR_TO_KVADDR(oldAddr), PAGE_SIZE);
		}

		for(unsigned int i = 0; i < new->as_npages2; i++){
			oldAddr = (paddr_t)array_get(old->pbase2_pt, i); 
			newAddr = (paddr_t)array_get(new->pbase2_pt, i); 
			memmove((void *)PADDR_TO_KVADDR(newAddr), (const void *)PADDR_TO_KVADDR(oldAddr), PAGE_SIZE);
		}

		for(int i = 0; i < DUMBVM_STACKPAGES; i++){
			oldAddr = (paddr_t)array_get(old->stackpbase_pt, i); 
			newAddr = (paddr_t)array_get(new->stackpbase_pt, i); 
			memmove((void *)PADDR_TO_KVADDR(newAddr), (const void *)PADDR_TO_KVADDR(oldAddr), PAGE_SIZE);
		}

	#else
		KASSERT(new->as_pbase1 != 0);
		KASSERT(new->as_pbase2 != 0);
		KASSERT(new->as_stackpbase != 0);

		memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

		memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
			(const void *)PADDR_TO_KVADDR(old->as_pbase2),
			old->as_npages2*PAGE_SIZE);

		memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
			(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
			DUMBVM_STACKPAGES*PAGE_SIZE);
	#endif
	
	*ret = new;
	return 0;
}
