#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include "opt-A2.h"

#if OPT_A2

#include <mips/trapframe.h> 
#include <synch.h>
//additional includes for execv
#include <vfs.h>
#include <kern/fcntl.h>

/* sys_fork() 
  - create process struct for childProc
  - proc_create_runprogram(…) to create - check for errors 
  - create and copy the address space using as_copy() - check for errors 
  - set address space that we copied to childProc process  
*/ 
int sys_fork(struct trapframe *tf, int *retval){

  char* childProc_name = kstrdup("childProcess");

  //create process struct for childProc
  struct proc * childProc = proc_create_runprogram(childProc_name); 
  //check for errors 
  if(childProc == NULL){
    //return error code from sysfork 
    return ENOMEM; 
  }
  //set return value of parent to be child pid 
  *retval = childProc->pid; 

  //copy parent address space using as_copy()
  spinlock_acquire(&childProc->p_lock);
    int childProc_as = as_copy(curproc->p_addrspace, &childProc->p_addrspace); 
    if(childProc_as != 0){
      return ENOMEM; 
    }
    // create parent child relationship 
    childProc->parent = curproc; 
    //add child to current children array 
    array_add(curproc->children, childProc, NULL);
  spinlock_release(&childProc->p_lock);     

  /*
  - assign PID to child process 
    - global counter - global counter in proc_bootstrap() 
  - this is done in proc_create(); 
  */

  //copy of parent tf -> kmalloc 
  //making a copy of the parents tf 
  struct trapframe *copyParentTf = kmalloc(sizeof(struct trapframe));

  *copyParentTf = *tf; 

  //create thread for child process 
  thread_fork("childThread", childProc, enter_forked_process, copyParentTf, (unsigned long)retval);

  return 0; 

}

//sys_getpid() returns the pid of the current process (yourself) 
int sys_getpid(pid_t *retval){
  spinlock_acquire(&curproc->p_lock);
  *retval = curproc->pid;
  spinlock_release(&curproc->p_lock);
  return 0; 
}

/* sys_waitpid() - need to return error codes? 
  - check if parent has this children using given pid
    - if call waitpid on a pid that is not child: return error code that says this is not my child
  - check if the child is alive or dead - add boolean to proc struct 
    - if child is alive: 
      - block current process using child's condition variable *add a cv to proc struc* (wait on child's cv)
      - after unblocked, get exit status and exit code, return the exit code 
      - delete your child 
    - if child is dead: 
      - get the exit code and exit status 
      - process these two using a macro to turn these two into a single integer 
      - delete the child - call proc destroy 
*/
int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval){
  int exitstatus;
  int result;
  
  bool isChildren = false; 
  struct proc * targetChild;
  int childIndex; 

  //check to see if curproc has a children with the given pid 
  for(int i = 0; i < (int)array_num(curproc->children); i++){
    if(((struct proc *)array_get(curproc->children,i))->pid == pid){
      isChildren = true; 
      targetChild = (struct proc *)array_get(curproc->children,i); //set get the current child that we want to work with 
      childIndex = i;
    }
  }
  //if we can't find a pid in the curproc's children -> this child is not a child of the parent
  if(isChildren == false){
    return ECHILD; //not my child error code 
  }

  //checked that pid is a child, now need to check if child is alive or dead 
  int childExitCode;

  //if the child is not alive
  if(targetChild->isAlive == false){
   
    //get the exit code and exit status of child 
    childExitCode = targetChild->exitCode; 

    //combine these two into one using macro 
    exitstatus = _MKWAIT_EXIT(childExitCode); 

    //call proc_destroy() to fully delete the child
    proc_destroy(targetChild);
    
    //remove the target child after deleting at index i
    array_remove(curproc->children, childIndex);

  }

  //if the child is alive
  else if(targetChild->isAlive == true){
    
    //while the child is still alive, we wait on the child to finish 
    lock_acquire(targetChild->procLock);
    while(targetChild->isAlive == true){
      cv_wait(targetChild->procCv, targetChild->procLock);
    }
    lock_release(targetChild->procLock); 

    //child is done exit, get exit code 
    childExitCode = targetChild->exitCode; 

    //combine these two into one using macro 
    exitstatus = _MKWAIT_EXIT(childExitCode); 

    //call proc_destroy() to fully delete the child
    proc_destroy(targetChild);

    array_remove(curproc->children, childIndex);
  }

  if (options != 0) {
    return(EINVAL);
  }
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

/* sys__exit() - child calling exit 
  - check if parent is alive 
    - if parent is alive: 
      - set child's exit code in structure aka. our own exit code, and exit status 
      - change cv to true -> to signal parent that I'm done doing my shiii 
    - if parent is dead: 
      - check if all my children are dead -> delete the dead ones, leave the alive ones alone 
      - kms 
*/
void sys__exit(int exitcode) {
  
  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
  * clear p_addrspace before calling as_destroy. Otherwise if
  * as_destroy sleeps (which is quite possible) when we
  * come back we'll be calling as_activate on a
  * half-destroyed address space. This tends to be
  * messily fatal.
  */
  as = curproc_setas(NULL);
  as_destroy(as); 

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  //check if parent is alive

  //if parent is alive  
  if(p->parent != NULL && p->parent->isAlive == true){
    
    //set the child's exit code, and signal parent that you're done shit 
    lock_acquire(p->procLock);
      p->exitCode = exitcode; 
      // set isAlive to false cause you've exited 
      p->isAlive = false; 
      cv_signal(p->procCv, p->procLock); 
    lock_release(p->procLock); 
    
  }
  
  //if the parent is not alive or is null pointer 
  else if(p->parent == NULL || p->parent->isAlive == false){
    //after dealing with children, kms 
    proc_destroy(p); 
  }

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

/* sys_execv() - replaces currently executing program with a newly loaded program 
  - count the number of arguments and copy them into the kernel 
  - copy the program path from user space into kernel (DONE) - Thursday Feb 18th 
*/
int sys_execv(userptr_t program, userptr_t arguments){
  struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	int totalArgs = 0; 

  // (void) arguments;  - storesd as a NULL terminated arrary of pointers to strings  copy individual strings from address space into the kernel 

  //copy program path from user space into kernel aka. copy program name into kernel space 
  //find size of program name 
  size_t programNameSize = sizeof(char) * (strlen((const char *)program) + 1); 
  //allocate space in the kernel for the name to live 
  char* kernelProgram = kmalloc(programNameSize); 
  if(kernelProgram == NULL){
    return ENOMEM;
  }
  int copyName = copyinstr(program, kernelProgram, programNameSize, NULL);
  // make sure that the copy is successful 
  if(copyName != 0){
    return ENOMEM;
  }

  /* 
  First phase: 
    - count the number of argumments and copy them from the existing address space of the current process into the kernel - ALLOCATE MEMORY FIRST 
    - need to use copyin each of the pointers to string 
    - allogcate memeory
    - copyinst the actual strings 
  */ 

  // count number of arguments - checked that it's working 
	char **args = (char **)arguments; 
	int i = 0; 
	while(args[i] != NULL){
		totalArgs += 1;
		i += 1; 
	}
	//kprintf(totalArgs); 

	// copying individual strings into the kernel 
	//allocate memory 
	size_t argumentSize = sizeof(char *) * (totalArgs + 1); //remember to include null character 
	char **kernelArgs = kmalloc(argumentSize); 
	//verify that we can allocate memory 
	if(kernelArgs == NULL){
		return ENOMEM; 
	}
	userptr_t usrPrtToString = NULL; 
	//start to copy each individual string 
	for(int i = 0; i < totalArgs; i++){
		//size of an individual string arg
		size_t individualArgSize = sizeof(char) * (strlen((const char*)args[i]) + 1); 
		//allocate memory for each individual string arg
		kernelArgs[i] = kmalloc(individualArgSize); 
		if(kernelArgs[i] == NULL){
			return ENOMEM; 
		}
		usrPrtToString = (userptr_t)args[i];
		int copyArgs = copyinstr(usrPrtToString, kernelArgs[i], individualArgSize, NULL);
		// make sure that the copy is successful 
		if(copyArgs != 0){
			return ENOMEM; 
		}
		//kprintf(kernelArgs[i]); 
	}
	//set last elem to NULL 
	kernelArgs[totalArgs] = NULL; 
	
  /* ============================================= copy from run_program ============================================= */
	/* Open the file. */
	result = vfs_open(kernelProgram, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	// KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
  // set the old address space 
	struct addrspace* old = curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}
 
	/* Done with the file now. */
	vfs_close(v);
  /* ============================================= copy from run_program ============================================= */

  //need to push argumnts on to the stack - own version of as-define stack - creates user stack and populate it with the arguments 

  //copy the arguments from the user space into the new addressspace 
	/* Define the user stack in the address space */
  vaddr_t usrSpaceAddrArgv;
	result = as_define_stack_new(as, &stackptr, kernelArgs, totalArgs, kernelProgram, &usrSpaceAddrArgv);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}
  
  //Delete the old address space 
  as_destroy(old); 
  kfree(kernelProgram);
  //FREE THE MEMORY ALLOCATED FOR STRINGS FOR ARGS 
  for(int i = 0; i < totalArgs; i++){
    kfree(kernelArgs[i]); 
  }
  kfree(kernelArgs); 

	//Call enter new process with address to the arguments on the stack, stack pointer, and program entry point 
  /* Warp to user mode. */
	//enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/, stackptr, entrypoint);

  enter_new_process(totalArgs /*argc*/, (userptr_t)usrSpaceAddrArgv /*userspace addr of argv*/, stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

}

/* ============================================================ OLD CODE ============================================================================ */
#else 

    /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

  void sys__exit(int exitcode) {

    struct addrspace *as;
    struct proc *p = curproc;
    /* for now, just include this to keep the compiler from complaining about
      an unused variable */
    (void)exitcode;

    DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

    KASSERT(curproc->p_addrspace != NULL);
    as_deactivate();
    /*
    * clear p_addrspace before calling as_destroy. Otherwise if
    * as_destroy sleeps (which is quite possible) when we
    * come back we'll be calling as_activate on a
    * half-destroyed address space. This tends to be
    * messily fatal.
    */
    as = curproc_setas(NULL);
    as_destroy(as); 

    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_remthread(curthread);

    /* if this is the last user process in the system, proc_destroy()
      will wake up the kernel menu thread */
    proc_destroy(p);
    
    thread_exit();
    /* thread_exit() does not return, so we should never get here */
    panic("return from thread_exit in sys_exit\n");
  }

  /* stub handler for getpid() system call                */
  int sys_getpid(pid_t *retval){
    /* for now, this is just a stub that always returns a PID of 1 */
    /* you need to fix this to make it work properly */
    *retval = 1;
    return(0);
  }

  /* stub handler for waitpid() system call                */
  int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval){
    int exitstatus;
    int result;

    /* this is just a stub implementation that always reports an
      exit status of 0, regardless of the actual exit status of
      the specified process.   
      In fact, this will return 0 even if the specified process
      is still running, and even if it never existed in the first place.

      Fix this!
    */
    if (options != 0) {
      return(EINVAL);
    }
    /* for now, just pretend the exitstatus is 0 */
    exitstatus = 0;
    result = copyout((void *)&exitstatus,status,sizeof(int));
    if (result) {
      return(result);
    }
    *retval = pid;
    return(0);
  
  }

#endif /* OPT_A2 */
