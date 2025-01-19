# Queue-based Notification System 
Project for studies which contains a queue-based notification system made with IPC mechanisms.

## Instruction
```bash
gcc inf160268_155228_d.c -o dispocitor && ./dispocitor <keyfile>
```

```bash
gcc inf160268_155228_p.c -o producer && ./producer <keyfile> 1 10
```

```bash
gcc inf160268_155228_k.c -o client && ./client keyfile.txt 1
```

For deleting processes:
```bash
ipcrm -a
```