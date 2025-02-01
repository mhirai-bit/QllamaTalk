# QllamaTalk

QllamaTalk is an experimental AI chatbot application that demonstrates how to integrate Qt, llama.cpp, and whisper.cpp in a single project.

Mac Usage Example  
![Mac Usage Example Screenshot](assets_for_readme/Desktop_Usage_Example.png)

iPhone Usage Example  
![iPhone Usage Example Screenshot](assets_for_readme/iPhone_Usage_Example.png)

---

## Environment

QllamaTalk has been tested on the following setups:

1. Windows 10 with Qt 6.8.1 (MSVC2022 64-bit)  
2. macOS (Sonoma 14.3.1) with Qt 6.8.1 for macOS  
3. Ubuntu 22.04.5 on VMWare with Qt 6.8.1 (Desktop Kit)  
4. iOS 17 on iPhone 13 mini with Qt 6.8.1 for iOS  
5. iOS 18 on iPhone 11 with Qt 6.8.1 for iOS  
6. Android 13 on Galaxy A22 5G with Android Qt 6.8.1 Clang arm64-v8a

---

## How to Build & Run

1. Clone this repository

    git clone https://github.com/mhirai-bit/QllamaTalk.git

2. Navigate to the QllamaTalk directory

    cd QllamaTalk

3. Open CMakeLists.txt in Qt Creator

    • Choose one of the Kits specified in the “Environment” section.  
    • The CMake configuration and generation process automatically updates the llama.cpp and whisper.cpp submodules and compiles them.  
      - On macOS, llama_setup.cmake enables Metal for llama inference.  
      - On other platforms, it defaults to CPU-based llama inference.  
        Note for other than macOS: CPU-only inference can be slow and may heavily use the CPU. If you want to enable GPU acceleration on another platform, refer to the llama.cpp build instructions (https://github.com/ggerganov/llama.cpp/blob/master/docs/build.md) and modify llama_setup.cmake accordingly.  
    • The CMake configuration and generation process also automatically downloads the default llama and whisper models.  
    • Android Note: Due to the model’s large size (about 1.6GB), bundling it in android/assets causes deployment failures. Instead, the app will download llama-3.1-8b-open-sft-q4_k_m.gguf from the internet on first launch. It also automatically downloads base.bin model for whisper.cpp.

4. Build and run the application

    In Qt Creator, press the Build and Run button (or use Ctrl+R or Cmd+R).

---

## Remote Server Feature

QllamaTalk supports both local and remote inference modes:

• Local Mode: Runs inference directly on your machine using llama.cpp.  
• Remote Mode: Connects to a remote server via Qt Web Sockets. The remote server also provides a Qt Remote Objects implementation, but the client (QllamaTalk) disables it by default. However, the Qt Remote Server implementation still works, so if you want to enable it, you can instantiate QtRemoteObjectsRemoteGenerator for mRemoteGenerator inside the constructor of RemoteResponseGeneratorCompositor. The remote server itself also uses llama.cpp, but runs in its own process or environment.

### Switching Between Local & Remote

In QML (or via your UI), you can switch between Local and Remote modes. If you supply ipAddress and portNumber, the app attempts to connect to a remote LLaMA server (for example: tcp://192.168.0.120:12345). The Qt Remote Objects implementation in the server listens on port 12345, and the Qt WebSockets implementation listens on port 12346. Multiple clients can connect to the latter.  

If the remote connection fails, QllamaTalk prompts you to fall back to local mode.

### Error Recovery

When an inference error occurs (for example, if tokenization or decoding fails), QllamaTalk attempts to:

1. Re-initialize the engine (either local or remote) in the background.  
2. If the error happens on the remote server, a signal is emitted to the client, and the client may:  
   - Stay in remote mode, waiting for the server to recover, or  
   - Fall back to local mode automatically (after some timeout).

On the server side, a function like reinitEngine() is called to free and recreate the LLaMA model context, ensuring recovery from a bad state. On the client side, LlamaChatEngine provides a similar reinitLocalEngine() if local inference fails.

---

### How to Build & Run the Server

You can open the CMakeLists.txt inside the LLMRemoteServer folder. This folder is a git submodule.  
This project has been tested on macOS (Sonoma 14.3.1) with Qt 6.8.1 for macOS. You can build and run this server on your Mac and access it from the client.

---

## Known Issues

1. The inference may occasionally produce garbled or irrelevant output. This might be due to the quality of the chosen model, but the exact cause is not fully understood yet.  
2. Under certain text inputs or conditions, the application may crash with the following error:  

    Qt has caught an exception thrown from an event handler. Throwing  
    exceptions from an event handler is not supported in Qt.  
    You must not let any exception whatsoever propagate through Qt code.  
    libc++abi: terminating due to uncaught exception of type std::invalid_argument: invalid character  

   This crash is inconsistent. It tends not to occur with English text but happens more frequently with Japanese text, suggesting a model-related issue with Japanese support.

---

## Future Plans

1. Support embedded Linux environments.  
2. Add a UI effect or 3D model that simulates an AI-bot talking on the screen.  
3. Investigate XR support.
