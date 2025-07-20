> First learn Computer Science and all the theory. Next develop a programming style. Then forget all that and just hack. - George Carrette

A "Game Engine" I am working on to help me create a Civilization-style game. In the past I would to try and write clean C++ code, before I realized that the language makes it impossible to do so.

# Building

### Setup

Firstly, make sure you have the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) installed. Next you want to initialize the other third-party dependencies.

```
git submodule init
git submodule update
```

Once this has been done, you can use CMake to build the project.

```
cmake -S . -B out
cd out && cmake --build .
```
