# Simple FSR integration into an ImGui OpenGL ES demo app


## Linux build & run

Before building make sure that the submodule(s) are correctly initialized:

``` sh
$ git submodule update --init 
```

To build and run in the project root dir

```sh
$ cmake -Bbuild -H.
$ make -C buld
$ ./build/gles_fsr <input image>
```
