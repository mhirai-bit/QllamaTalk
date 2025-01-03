# QllamaTalk
QllamaTalk is an experimental AI chatbot application that demonstrates how to integrate Qt and llama in a single project.<br>
*Mac Usage Example*<br>
![Mac Usage Example Screenshot](assets_for_readme/Desktop_Usage_Example.png)
<br>*iPhone Usage Example*<br>
![IPhone Usage Example Screenshot](assets_for_readme/iPhone_Usage_Example.png)

## Environment
QllamaTalk has been tested on the following setups:
1. **Windows 10** with **Qt 6.8.1 (MSVC2022 64-bit)**  
2. **macOS (Sonoma 14.3.1)** with **Qt 6.8.1 for macOS**  
3. **Ubuntu 22.04.5** on VMWare with **Qt 6.8.1 (Desktop Kit)**  
4. **iOS 17** with iPhone 13 mini with **Qt 6.8.1 for iOS**
5. **iOS 18** with iPhone 11 with **Qt 6.8.1 for iOS**


## How to Build & Run
1. **Clone this repository**  
   ```bash
   git clone https://github.com/mhirai-bit/QllamaTalk.git
   ```

2. **Navigate to the QllamaTalk directory**  
   ```bash
   cd QllamaTalk
   ```

3. **Open `CMakeLists.txt` in Qt Creator**  
   - Choose one of the Kits specified in the “Environment” section.
   - The cmake configuration and generation process automatically updates the `llama.cpp` submodule and compiles it.
     - On macOS and iOS, [llama_setup.cmake](cmake/llama_setup.cmake) enables Metal for inference.
     - On other platforms, it defaults to CPU-based inference.  
     - **Note (for other than macOS and iOS)**: CPU-only inference can be slow and may heavily use the CPU. If you want to enable GPU acceleration on another platform, refer to the [llama.cpp build instructions](https://github.com/ggerganov/llama.cpp/blob/master/docs/build.md) and modify `llama_setup.cmake` accordingly.
   - The cmake configuration and generation process also automatically downloads the default model.
   
4. **Build and run the application**  
   - In Qt Creator, press the **Build and Run** button (or use the **Ctrl+R** / **Cmd+R** shortcut).  

## Known Issues
1. The inference may occasionally produce garbled or irrelevant output. This might be due to the quality of the chosen model, but the exact cause is still unclear.
2. Under certain text inputs or conditions, the application can crash with the following error:
   ```
   Qt has caught an exception thrown from an event handler. Throwing
   exceptions from an event handler is not supported in Qt.
   You must not let any exception whatsoever propagate through Qt code.
   libc++abi: terminating due to uncaught exception of type std::invalid_argument: invalid character
   ```
   The crash is not consistently reproducible with the same text, so the cause remains uncertain.

## Future Plans
1. Support Android
2. Support iOS with remote inference
2. Support embedded Linux environments
3. Add voice input and output functionality
