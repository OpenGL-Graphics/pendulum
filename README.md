# Screenshot
![Screenshot](/screenshots/screenshot-2023-04-11.png)

# Description
Simple pendulum made out of a moving sphere rendered with OpenGL, and based on [this video][1] from The Coding Train.

[1]: https://www.youtube.com/watch?v=NBWMtlbbOag

# How to run
```console
# clone repo with its submodules
$ git clone --recursive https://github.com/OpenGL-Graphics/pendulum

# build & run
$ cd terrain
$ mkdir build && cd build
$ cmake .. && make -j && ./main

# to get new commits from submodules
$ git submodule update --remote
```
