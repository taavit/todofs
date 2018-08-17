# TodoFS
Some excercises/trials with c and libfuse.

Only reading title supported at this stage.

## Requirements:
* libfuse
* libjson-c
### Ubuntu
  ```$ sudo apt install libfuse-dev libjson-c-dev```
## Build:
  * ```gcc -Wall src/read_json.c `pkg-config fuse json-c --cflags --libs` -o bin/todofs```

or

  * ```make```
## Usage
```./bin/todofs mount_point -o filename=assets/todos.json```
