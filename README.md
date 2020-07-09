# ImGui OpenGL ES skeleton

A skeleton project which integrates ImGui with OpenGL ES (3.0+) using GLFW.


## Linux build & run

Before building make sure that the submodule(s) are correctly initialized:

``` sh
$ git submodule update --init 
```

To build and run:

```sh
$ cmake -Bbuild -H.
$ make -C buld
$ ./build/src/skeleton
```
