#include "compiler.h"
#include "../config.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <memory>
#include <vector>
#include <windows.h>
#include <commdlg.h>

static std::map<std::string, HINSTANCE> loadedHandles;

static std::string collectStaticLinks(const std::string& cppCode)
{
        const std::string tag = "// SULLA_STATIC_LINK ";
        std::string extra;
        std::set<std::string> seen;
        std::istringstream iss(cppCode);
        std::string line;
        while (std::getline(iss, line))
        {
                if (line.rfind(tag, 0) != 0) continue;
                std::string path = line.substr(tag.size());
                while (!path.empty() && (path.back() == '\r' || path.back() == ' ')) path.pop_back();
                if (!path.empty() && seen.insert(path).second && std::filesystem::exists(path))
                        extra += " " + path;
        }
        return extra;
}

static std::string sullaPartCompiler()
{
        static std::string cached;
        static bool detected = false;
        if (detected) return cached;
        detected = true;
        const char* candidates[] = { PART_COMPILER, "clang++", "g++", "c++" };
        for (const char* c : candidates)
        {
                if (std::system((std::string(c) + " --version > NUL 2>&1").c_str()) == 0) { cached = c; break; }
        }
        return cached;
}

bool compileSharedLibrary(const std::string& cppCode, const std::string& moduleName)
{
        std::string cc = sullaPartCompiler();
        if (cc.empty()) return false;
        if (!std::filesystem::exists("parts")) std::filesystem::create_directory("parts");

        std::string srcFile = "parts/" + moduleName + ".cpp";
        std::string outFile = "parts/lib" + moduleName + ".dll";

        std::ofstream out(srcFile);
        out << cppCode;
        out.close();

        std::string staticLibs = collectStaticLinks(cppCode);
        std::string command = cc + " -O3 -shared " + srcFile + staticLibs + " -o " + outFile;
        int result = std::system(command.c_str());

        std::filesystem::remove(srcFile);

        return (result == 0);
}

void unloadCompiledPart(const std::string& moduleName)
{
        std::map<std::string, HINSTANCE>::iterator it = loadedHandles.find(moduleName);
        if (it != loadedHandles.end())
        {
                FreeLibrary(it->second);
                loadedHandles.erase(it);
        }
}

bool compilePartLibrary(const std::string& cppCode, const std::string& label,
                        bool buildStatic, bool buildDynamic)
{
        std::string cc = sullaPartCompiler();
        if (cc.empty()) return false;
        std::string dir = sullaPartDir(label);
        std::filesystem::create_directories(dir);
        std::string staticLibs = collectStaticLinks(cppCode);
        bool ok = true;

        if (buildDynamic)
        {
                std::string srcFile = dir + "/" + label + ".dyn.cpp";
                std::string outFile = dir + "/lib" + label + ".dll";
                std::ofstream(srcFile) << cppCode;
                std::string command = cc + " -O3 -shared " + srcFile + staticLibs + " -o " + outFile;
                ok = (std::system(command.c_str()) == 0) && ok;
                std::filesystem::remove(srcFile);
        }

        if (buildStatic)
        {
                std::string code = cppCode;
                const std::string from = "void executeTick(";
                std::string::size_type pos = code.find(from);
                if (pos != std::string::npos)
                        code.replace(pos, from.size(), "void " + sullaPartSymbol(label) + "(");

                std::string srcFile = dir + "/" + label + ".sta.cpp";
                std::string objFile = dir + "/" + label + ".o";
                std::string arFile  = dir + "/lib" + label + ".a";
                std::ofstream(srcFile) << code;
                int r1 = std::system((cc + " -O3 -c " + srcFile + " -o " + objFile).c_str());
                std::error_code ec; std::filesystem::remove(arFile, ec);
                int r2 = std::system(("ar rcs " + arFile + " " + objFile).c_str());
                std::filesystem::remove(srcFile);
                std::filesystem::remove(objFile);
                ok = (r1 == 0 && r2 == 0) && ok;
        }

        {
                std::error_code ec;
                if (sullaCodeIsStateless(cppCode))
                        std::ofstream(sullaStatelessMarker(label)) << "1";
                else
                        std::filesystem::remove(sullaStatelessMarker(label), ec);
        }

        return ok;
}

Part loadCompiledPart(const std::string& moduleName, int outCount)
{
        unloadCompiledPart(moduleName);

        std::string resolved = sullaFindDynamic(moduleName);
        std::string libPath = !resolved.empty() ? ("./" + resolved)
                                                : ("./parts/lib" + moduleName + ".dll");
        HINSTANCE handle = LoadLibraryA(libPath.c_str());
        if (!handle) return nullptr;

        loadedHandles[moduleName] = handle;

        RawTickFn executeTick = (RawTickFn)GetProcAddress(handle, "executeTick");

        if (!executeTick) return nullptr;

        struct RawScratch { std::vector<uint8_t> rawIn, rawOut; };
        auto scratch = std::make_shared<RawScratch>();
        scratch->rawOut.assign(outCount, 0);

        return [executeTick, outCount, scratch](const std::vector<State>& inputs) -> std::vector<State> {
                std::vector<uint8_t>& rawIn = scratch->rawIn;
                std::vector<uint8_t>& rawOut = scratch->rawOut;
                if (rawIn.size() < inputs.size()) rawIn.resize(inputs.size());
                for (size_t i = 0; i < inputs.size(); ++i) rawIn[i] = (inputs[i] == STATE_HIGH) ? 1 : 0;

                executeTick(rawIn.data(), rawOut.data());

                std::vector<State> outputs(outCount);
                for (size_t i = 0; i < (size_t)outCount; ++i) outputs[i] = rawOut[i] ? STATE_HIGH : STATE_LOW;
                return outputs;
        };
}

RawTickFn loadRawTick(const std::string& moduleName)
{
        std::map<std::string, HINSTANCE>::iterator it = loadedHandles.find(moduleName);
        HINSTANCE handle = (it != loadedHandles.end()) ? it->second : nullptr;
        if (!handle)
        {
                std::string resolved = sullaFindDynamic(moduleName);
                std::string libPath = !resolved.empty() ? ("./" + resolved)
                                                        : ("./parts/lib" + moduleName + ".dll");
                handle = LoadLibraryA(libPath.c_str());
                if (!handle) return nullptr;
                loadedHandles[moduleName] = handle;
        }
        return (RawTickFn)GetProcAddress(handle, "executeTick");
}

bool hasNativeFileDialog()
{
        return true;
}

std::string openNativeFileDialog(const std::string& title, const std::string& filterName, const std::string& filterPattern)
{
        char filename[MAX_PATH] = "";
        std::string filter = filterName + '\0' + filterPattern + '\0' + "All Files" + '\0' + "*.*" + '\0';
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title.c_str();
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) return std::string(filename);
        return "";
}
