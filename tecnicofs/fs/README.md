## Threads
### Implementing Strategy

#### Local critic spots with memory acesses
- Create a local variable to only acess twice to the shared state : at the beggining and at the end
- During the function execution, work with the local variables that aren't shared to other in-execution threads

####