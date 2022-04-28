// SpvToHeaderConverter
// Converts all spv files in directory to one C++ ixx module file
// Can run glslc compiler for creating .spv files (optional)

#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <array>
#include <vector>
#include <deque>
#include <optional>
#include <cstring>

#include "Headers/spdlog/spdlog/spdlog.h"
#include "Headers/spdlog/spdlog/sinks/stdout_color_sinks.h"
#include "Headers/spdlog/spdlog/sinks/basic_file_sink.h"

#if defined(_WIN32) || defined(WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;
#endif

#if defined(_MSC_VER)

#define _ALWAYS_INLINE __forceinline
#define PARSESTR std::wstring

#elif defined(__clang__ || __GNUC__)

#define _ALWAYS_INLINE __attribute__((always_inline)) __inline__
#define PARSESTR std::string

#elif

#define _ALWAYS_INLINE inline
#define PARSESTR std::string

#endif

constexpr std::array<const char*, 13> shadertypes =
    { ".vert", ".frag", ".comp", ".tesc", ".tese", ".rgen", ".rchit",
    ".rahit", ".rmiss", ".rint", ".rcall", ".mesh", ".task" };

class ConfigParser final {
public:
    explicit ConfigParser() {
        std::ifstream cfgfile{ "SpvToHeaderConverter.config",
                    std::ios::in | std::ios::ate | std::ios::binary };
        cfgfile.exceptions(cfgfile.failbit);
        const std::streamsize size = cfgfile.tellg();
        cfgfile.seekg(0, std::ios::beg);

        std::vector<char> cfgbuffer(size + 1);
        cfgbuffer.shrink_to_fit();
        cfgbuffer.back() = '\0';
        if (cfgfile.read(cfgbuffer.data(), size)) {
            std::istringstream inbuf(cfgbuffer.data());
            for (std::array<char, 300> strline; inbuf.getline(&strline[0], 300, '\n'); ) {
                std::istringstream strbuf(strline.data());
                bool iscommand = false;
                std::pair<std::string, std::string> compair;
                for (std::array<char, 200> stritem; strbuf.getline(&stritem[0], 200, '='); ) {
                    iscommand = !iscommand;
                    std::istringstream strclear(stritem.data());
                    std::string itemclear;
                    strclear >> itemclear;
                    if (iscommand) {
                        compair.first = itemclear;
                    }
                    else {
                        compair.second = itemclear;
                    }
                }
                if (compair.first.empty() || compair.second.empty()) {
                    continue;
                }
                else {
                    CheckAndSetOneStrParameters(compair);
                }
            }
        }
    }

    ConfigParser(const ConfigParser&) = delete;
    ConfigParser(const ConfigParser&&) = delete;
    ConfigParser& operator=(const ConfigParser&) = delete;
    ConfigParser& operator=(const ConfigParser&&) = delete;

    _ALWAYS_INLINE std::optional<std::filesystem::path> GetGlslcPath() const noexcept {
        return m_glslc_path;
    }

    _ALWAYS_INLINE std::optional<std::filesystem::path> GetSaveModulePath() const noexcept {
        return m_save_module_path;
    }

private:
    template<typename T>
        requires requires {
        std::is_same<T, std::string>::value;
    }
    [[nodiscard]] std::optional<int> FindCommand(const std::string& commandstr) const noexcept {
        int i{ 0 };
        if constexpr (std::is_same<T, std::string>::value) {
            for (const auto& typecommandstr : paircommandpathrefs) {
                if (typecommandstr.first == commandstr) {
                    return i;
                }
                i++;
            }
        }
        return std::nullopt;
    }

    void CheckAndSetOneStrParameters(const std::pair<std::string, std::string>& compair) {
        const std::optional<int> comminindexstr = FindCommand<std::string>(compair.first);
        if (comminindexstr.has_value()) {
            if (paircommandpathrefs[comminindexstr.value()].second.get().has_value()) {
                std::string errstr = "Double command ";
                errstr += paircommandpathrefs[comminindexstr.value()].first;
                errstr += " error";
                throw std::runtime_error(errstr.c_str());
            }
            else {
                paircommandpathrefs[comminindexstr.value()].second.get() = compair.second;
            }
            return;
        }        
        throw std::runtime_error("Unknown command type");
    }

    std::optional<std::filesystem::path> m_glslc_path;
    std::optional<std::filesystem::path> m_save_module_path;
    const std::array<std::pair<const char*, std::reference_wrapper<std::optional<std::filesystem::path>>>, 2> paircommandpathrefs
    { { {"glslc_path", m_glslc_path}, {"save_module_path",  m_save_module_path} } };
};


template<typename S>
    requires requires {
    std::is_same<S, std::string>::value || std::is_same<S, std::wstring>::value;
}
class CommandLineParser final {
public:
    explicit CommandLineParser(const int argc, const char* argv[]) {
        int i{ 1 };
        while (i < argc) {
            if (InitCommandBool(argv, i)) {
                i++;
            }
            else if (InitCommandString(argc, argv, i)) {
                i += 2;
            }
            else {
                throw std::runtime_error("Unknown command or argument");
            }
        }
    }

    CommandLineParser(const CommandLineParser&) = delete;
    CommandLineParser(const CommandLineParser&&) = delete;
    CommandLineParser& operator=(const CommandLineParser&) = delete;
    CommandLineParser& operator=(const CommandLineParser&&) = delete;

    _ALWAYS_INLINE std::optional<bool> GetIsCompileAll() const noexcept {
        return m_iscompileall;
    }

    _ALWAYS_INLINE std::optional<std::deque<S>> GetToCompileList() const noexcept {
        return m_tocompilelist;
    }

private:
    template<typename T>
        requires requires {
        std::is_same<T, std::string>::value || std::is_same<T, bool>::value;
    }
    [[nodiscard]] std::optional<int> FindCommand(const char* commandstr) const noexcept {
        int i{ 0 };
        if constexpr (std::is_same<T, std::string>::value) {
            for (const auto& typecommandstr : paircommandstringrefs) {
                if (std::strcmp(typecommandstr.first, commandstr) == 0) {
                    return i;
                }
                i++;
            }
        }
        else if constexpr (std::is_same<T, bool>::value) {
            for (const auto& typecommandstr : paircommandboolrefs) {
                if (std::strcmp(typecommandstr.first, commandstr) == 0) {
                    return i;
                }
                i++;
            }
        }
        return std::nullopt;
    }

    bool InitCommandString(const int argc, const char* argv[], const int i) {
        const std::optional<int> commindex = FindCommand<std::string>(argv[i]);
        if (!commindex.has_value()) {
            return false;
        }
        else {
            if (paircommandstringrefs[commindex.value()].second.get().has_value()) {
                std::string errstr = "Double command ";
                errstr += paircommandstringrefs[commindex.value()].first;
                errstr += " error";
                throw std::runtime_error(errstr.c_str());
            }
            else {
                paircommandstringrefs[commindex.value()].second.get() = std::deque<S>(0);
                std::deque<S>& tempdeq{ paircommandstringrefs[commindex.value()].second.get().value() };
                const int comnext{ i + 1 };                
                if (comnext < argc) {
                    std::stringstream strbuf{ argv[comnext] };
                    for (std::array<char, 70> strline; strbuf.getline(&strline[0], 70, ','); ) {
                        const std::string toaddstr(strline.data());
                        if constexpr (std::is_same<S, std::wstring>::value) {
                            const std::wstring wsTmp(toaddstr.begin(), toaddstr.end());
                            tempdeq.emplace_back(wsTmp);
                        }
                        else {
                            tempdeq.emplace_back(toaddstr);
                        }
                    }
                    return true;
                }
                else {
                    const std::string err = std::string(paircommandstringrefs[commindex.value()].first) + " have not argument";
                    throw std::runtime_error(err.c_str());
                }                
            }
        }
    }

    bool InitCommandBool(const char* argv[], const int i) {
        const std::optional<int> commindex = FindCommand<bool>(argv[i]);
        if (!commindex.has_value()) {
            return false;
        }
        else {
            if (paircommandboolrefs[commindex.value()].second.get().has_value()) {
                std::string errstr = "Double command ";
                errstr += paircommandboolrefs[commindex.value()].first;
                errstr += " error";
                throw std::runtime_error(errstr.c_str());
            }
            else {
                paircommandboolrefs[commindex.value()].second.get() = true;
                return true;
            }
        }
    }

    std::optional<bool> m_iscompileall;
    std::optional<std::deque<S>> m_tocompilelist;
    const std::array<std::pair<const char*,
        std::reference_wrapper<std::optional<bool>>>, 1> paircommandboolrefs
    { { {"-compile_all", m_iscompileall} } };
    const std::array<std::pair<const char*,
        std::reference_wrapper<std::optional<std::deque<S>>>>, 1> paircommandstringrefs
    { { {"-compile_files", m_tocompilelist} } };
};

template<typename T>
    requires requires {
    std::is_same<T, std::string>::value || std::is_same<T, std::wstring>::value;
}
void CreateListWithShaderSources(std::deque<T>& list) {
    const std::filesystem::path basepath{ std::filesystem::current_path() };

    for (auto const& dir_entry : std::filesystem::directory_iterator{ basepath }) {
        if (dir_entry.is_regular_file()) {
            std::filesystem::path filepath{ dir_entry };
            
            for (auto const& shadertype : shadertypes) {
                if (filepath.extension() == shadertype) {
                    if constexpr (std::is_same<T, std::wstring>::value) {
                        list.emplace_back(filepath.filename().wstring());
                    }
                    else if constexpr (std::is_same<T, std::string>::value) {
                        list.emplace_back(filepath.filename().string());
                    }
                }
            }            
        }
    }
}


#if defined(_WIN32) || defined(WIN32)
bool RunGlslcProcess(const std::optional<std::filesystem::path>& glslc_path,
                     const std::optional<bool>& iscompileall,
                     const std::optional<std::deque<std::wstring>>& tocompilelist) noexcept {
    std::wstring commandline_str{ glslc_path.has_value() ? glslc_path.value().wstring()
                                                + L"glslc.exe -c -O" : L"glslc.exe -c -O" };
    std::deque<std::wstring> listofshaders;
    const std::wstring basepathstr{ std::filesystem::current_path().wstring()};    
    if (iscompileall.has_value()) {
        CreateListWithShaderSources(listofshaders);
        for (auto const& shaderstr : listofshaders) {
            commandline_str += glslc_path.has_value() ? L" " +
                basepathstr + L"\\" + shaderstr : L" " + shaderstr; 
        }
    }
    else if (tocompilelist.has_value()) {
        for (auto const& shaderstr : tocompilelist.value()) {
            commandline_str += glslc_path.has_value() ? L" " +
                basepathstr + L"\\" + shaderstr : L" " + shaderstr;
        }
    }
    else {
        spdlog::error("No coommand for glslc operation");
        return false;
    }

    STARTUPINFOW si{
        .cb = sizeof(si)
    };
    PROCESS_INFORMATION pi{};    

    if (!CreateProcessW(nullptr, const_cast<wchar_t*>(commandline_str.c_str()),
                        nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi)) {
        spdlog::critical("CreateProcess failed: {0}", GetLastError());
        return false;
    }

    const DWORD status = WaitForSingleObject(pi.hProcess, INFINITE);
    bool result{ true };
    switch (status) {
    case WAIT_ABANDONED: 
        spdlog::error("GLSLC exited with status: WAIT_ABANDONED");
        result = false;
        break;
    case WAIT_OBJECT_0:
        spdlog::info("GLSLC exited with status: WAIT_OBJECT_0");
        result = true;
        break;
    case WAIT_TIMEOUT:
        spdlog::error("GLSLC exited with status: WAIT_TIMEOUT");
        result = false;
        break;
    case WAIT_FAILED:
        spdlog::error("GLSLC exited with status: WAIT_FAILED");
        result = false;
        break;
    default:        
        spdlog::error("GLSLC exited with status: unknown type of status");
        result = false;
        break;
    }  

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return result;
}
#else
bool RunGlslcProcess(const std::optional<std::filesystem::path>& glslc_path,
                     const std::optional<bool>& iscompileall,
                     const std::optional<std::deque<std::string>>& tocompilelist) noexcept {

    pid_t pid;
    std::deque<std::string> listofshaders;
    const std::string gl_path_param = glslc_path.has_value() ?
        glslc_path.value().string() : "/bin/glslc";
    const char* c_param{ "-c" };
    const char* o_param{ "-0" };
    std::vector<char*> vec_argv{
        const_cast<char*>(gl_path_param.c_str()),
        const_cast<char*>(c_param),
        const_cast<char*>(o_param),
    };    

    if (iscompileall.has_value()) {
        CreateListWithShaderSources(listofshaders);
        for (auto const& shaderstr : listofshaders) {
            vec_argv.emplace_back(const_cast<char*>(shaderstr.c_str()));
        }
    }
    else if (tocompilelist.has_value()) {
        for (auto const& shaderstr : tocompilelist.value()) {
            vec_argv.emplace_back(const_cast<char*>(shaderstr.c_str()));
        }
    }
    else {
        spdlog::error("No coommand for glslc operation");
        return false;
    }
    vec_argv.emplace_back(nullptr);
    
    fflush(nullptr);
    bool result{ true };
    int status = posix_spawn(&pid, gl_path_param.c_str(), nullptr, nullptr, vec_argv.data(), environ);
    if (status == 0) {
        fflush(nullptr);
        if (waitpid(pid, &status, 0) != -1) {
            spdlog::info("GLSLC exited with status: {0}", strerror(status));
            result = true;
        }
        else {
            perror("waitpid");
            spdlog::error("GLSLC exited with status: {0}", strerror(status));
            result = false;
        }
    }
    else {
        spdlog::critical("CreateProcess failed: {0}", strerror(status));
        return false;
    }
    return result;
}
#endif

[[nodiscard]] inline std::string convert_correct_string(const std::uint8_t a) noexcept {
    std::stringstream buf;
    buf << std::hex << static_cast<unsigned int>(a);
    const std::string temp = buf.str();
    const std::size_t len = temp.length();
    std::string final = "0x";
    final += (len == 1) ? "0" + temp : temp;
    return final;
}

void createModuleFromSpvFiles(const std::optional<std::filesystem::path>& save_module_path) {
    const std::filesystem::path basepath{ std::filesystem::current_path() };

    std::stringstream finalbuf;

    finalbuf << "export module shader_spv;\n\nimport <array>;\n\n";
    finalbuf << "namespace OnyWarp\n{\n";

    constexpr int num_in_str = 16;

    for (auto const& dir_entry : std::filesystem::directory_iterator{ basepath }) {
        if (dir_entry.is_regular_file()) {
            std::filesystem::path filepath{ dir_entry };
            if (filepath.extension() == ".spv") {
                if (std::ifstream filespv{ filepath, std::ios::in | std::ios::ate | std::ios::binary }) {
                    std::size_t filesize = filespv.tellg();
                    filespv.seekg(0, std::ios::beg);

                    std::string nameofdata = filepath.stem().string();
                    std::replace(nameofdata.begin(), nameofdata.end(), '.', '_');

                    finalbuf << "\texport constinit std::array<const unsigned char, " << filesize << "> "
                        << nameofdata << "_bytecode\n\t{\n\t\t";

                    std::uint8_t read_symbol;
                    int cur_num = 0;

                    while (filesize != 0) {
                        filespv.read(reinterpret_cast<char*>(&read_symbol), sizeof read_symbol);
                        finalbuf << convert_correct_string(read_symbol) << ",";
                        if (++cur_num == num_in_str) {
                            finalbuf << "\n\t\t";
                            cur_num = 0;
                        }
                        filesize--;
                    }
                }
                finalbuf << "\n\t};\n";
            }
        }
    }
    finalbuf << "}";
    //std::cout << finalbuf.str();
#if defined(_MSC_VER)
    const std::wstring writefilename = save_module_path.has_value() ?
        save_module_path.value().wstring() + L"shader_spv.ixx" : L"shader_spv.ixx";
#else
    const std::string writefilename = save_module_path.has_value() ?
        save_module_path.value().string() + "shader_spv.ixx" : "shader_spv.ixx";
#endif
    std::ofstream ostrm(writefilename, std::ios::out);
    try {
        ostrm.exceptions(ostrm.failbit);
    }
    catch (const std::ios_base::failure& ex) {
        spdlog::critical("Unable to create stream for writing"
                        " ixx file with error {0}", ex.what());
    }
    const std::string finalstr = finalbuf.str();
    ostrm.write(finalstr.c_str(), finalstr.size());
    spdlog::info("All done");
}

int main(int argc, char* argv[]) {
    std::optional<bool> iscompileall;
    std::optional<std::deque<PARSESTR>> tocompilelist;
   
    if (argc > 1) {
        try {
            CommandLineParser<PARSESTR> parser(argc, const_cast<const char**>(argv));           
            iscompileall = parser.GetIsCompileAll();
            tocompilelist = parser.GetToCompileList();
            if (iscompileall.has_value() and tocompilelist.has_value()) {
                throw std::runtime_error("compile_all and compile_files can't"
                                                            " be set simultaneously");
            }
        }
        catch (const std::runtime_error& ex) {
            spdlog::warn("Command line parse problem: {0}. Programm will"
                        " not compile shaders, only create module", ex.what());
            iscompileall.reset();
            tocompilelist.reset();
        }
    }

    std::optional<std::filesystem::path> glslc_path;
    std::optional<std::filesystem::path> save_module_path;
    try {
        ConfigParser parser;
        glslc_path = parser.GetGlslcPath();
        save_module_path = parser.GetSaveModulePath();
    }
    catch (const std::ios_base::failure& ex) {
        spdlog::warn("Exception opening/reading/closing configuration"
                        " file: {0}. Program will use default parameters", ex.what());
        glslc_path.reset();
        save_module_path.reset();
    }
    catch (const std::runtime_error& ex) {
        spdlog::warn("Unable to parse configuration file: {0}."
                        " Program will use default parameters", ex.what());
        glslc_path.reset();
        save_module_path.reset();
    }
    
    bool glslresult{ true };
    if (iscompileall.has_value() or tocompilelist.has_value()) {
        glslresult = RunGlslcProcess(glslc_path, iscompileall, tocompilelist);
    }
    if (glslresult) {
        createModuleFromSpvFiles(save_module_path);
    }
    else {
        spdlog::warn("All done without creating module file");
    }
}