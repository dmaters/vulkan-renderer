# Vulkan Renderer

A Vulkan-based renderer designed for experimenting and testing various graphics techniques. The project aims to provide
a modular and extensible framework for rapid prototyping and development of rendering techniques.

## Build Instructions

### Requirements

- C++20 or later
- Vulkan SDK 1.4 + (earlier versions are untested)
- CMake (version 3.7+)

### Building

```sh
mkdir build && cd build
cmake .. 
cmake --build .
```

Run the executable:

```bash
    ./build/Renderer {path to obj}
```

This project has been primarily tested with
the [Fox](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Fox)
and [Sponza](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Sponza) models from Khronos.

It's advised to use those models to avoid unexpected behaviours while testing the build.

The project is designed to support a wide range of glTF models.
If you encounter any issues while loading other models, please consider creating an issue to help improve compatibility.

## Screenshots

### Fox

<img width="1917" height="1034" alt="image" src="https://github.com/user-attachments/assets/09c78de5-6677-4885-8bcc-f22e8687b8c2" />


### Sponza

<img width="1917" height="1034" alt="image" src="https://github.com/user-attachments/assets/be5f4cb4-5532-48eb-a049-bdd484a3edf3" />



## Contributing

The project is in a very early stage. If you encounter any errors, compatibility issues, or have suggestions, please
feel free to open an issue or submit a pull request.


