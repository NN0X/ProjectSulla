#include "compiler.h"

#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <windows.h>

std::string getExportSignature()
{
        return "__declspec(dllexport) void executeTick(const uint8_t* in, uint8_t* out) {\n";
}

bool checkCompilerAvailability()
{
        int res = std::system("tcc -v > NUL 2>&1");
        return (res == 0);
}

bool compileSharedLibrary(const std::string& cCode, const std::string& moduleName)
{
        if (!std::filesystem::exists("parts")) std::filesystem::create_directory("parts");

        std::string srcFile = "parts/" + moduleName + ".c";
        std::string outFile = "parts/lib" + moduleName + ".dll";

        std::ofstream out(srcFile);
        out << cCode;
        out.close();

        std::string command = "tcc -shared " + srcFile + " -o " + outFile;
        int result = std::system(command.c_str());

        std::filesystem::remove(srcFile);

        return (result == 0);
}

Part loadCompiledPart(const std::string& moduleName, int outCount)
{
        std::string libPath = "./parts/lib" + moduleName + ".dll";
        HINSTANCE handle = LoadLibraryA(libPath.c_str());
        if (!handle) return nullptr;

        void (*executeTick)(const uint8_t*, uint8_t*) = (void(*)(const uint8_t*, uint8_t*))GetProcAddress(handle, "executeTick");

        if (!executeTick) return nullptr;

        return [executeTick, outCount](std::vector<State> inputs) -> std::vector<State> {
                std::vector<uint8_t> rawIn(inputs.size());
                for(size_t i = 0; i < inputs.size(); ++i) rawIn[i] = inputs[i] == STATE_HIGH ? 1 : 0;

                std::vector<uint8_t> rawOut(outCount); 
                executeTick(rawIn.data(), rawOut.data());

                std::vector<State> outputs(outCount);
                for(size_t i = 0; i < (size_t)outCount; ++i) outputs[i] = rawOut[i] == 1 ? STATE_HIGH : STATE_LOW;

                return outputs;
        };
}
