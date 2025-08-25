# Vulkan Renderer

A Vulkan-based renderer designed for experimenting and testing various graphics techniques. The project aims to provide a modular and extensible framework for rapid prototyping and development of rendering techniques.


## Build Instructions

### Requirements

- C++20 or later
- Vulkan 1.4 SDK 
- CMake (version 3.7+)

### Building
```sh
mkdir build && cd build
cmake .. 
cmake --build .
```

Run the executable:

```bash
    ./Renderer {path to obj}
```

This project has been primarily tested with the [Fox](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Fox) and [Sponza](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Sponza) models from Khronos. 

It's adviced to use those models to avoid unexpected behaviours while testing the build.

The project is designed to support a wide range of glTF models.
If you encounter any issues while loading other models, please consider creating an issue to help improve compatibility.


## Contributing
The project is in a very early stage. If you encounter any errors, compatibility issues, or have suggestions, please feel free to open an issue or submit a pull request.


