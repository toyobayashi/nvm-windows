#include <Windows.h>
#include <stdio.h>
#include <json/json.hpp>
#include <iostream>
#include <fstream>
#include "util.h"
#include "path.hpp"
#include "cli.h"
#include "download.h"
#include "config.h"
#include "progress.h"

#define NVM_VERSION "1.1.0"

typedef struct unzipparam {
   ProgressBar* prog;
   std::wstring rootname;
} unzipparam;

static void onUnzip(Util::unzCallbackInfo* info, void* param) {
  unzipparam* p = (unzipparam*)param;
  if (p->rootname == L"") {
    auto i = info->name.find(L"\\");
    if (i != std::wstring::npos) {
      p->rootname = info->name.substr(0, i);
    }
  }
  
  p->prog->setRange(0, info->total);
  p->prog->setPos(info->uncompressed);
  p->prog->print();
}

static void onDownload(progressInfo* info, void* param) {
  ProgressBar* prog = (ProgressBar*)param;
  prog->setRange(0, info->total);
  prog->setBase(info->size);
  prog->setPos(info->sum);
  prog->print();
  // Util::clearLine(0);
  // printf("Downloading %s: %.2lf%%...", Util::w2a(*((std::wstring*)param)).c_str(), 100 * (double)(info->size + info->sum) / info->total);
}

static size_t onResponse(void* buffer, size_t size, size_t nmemb, std::string* userp) {
  char* tmp = new char[size * nmemb + 1];
  memset(tmp, 0, size * nmemb + 1);
  lstrcpyA(tmp, (char*)buffer);
  *userp += tmp;
  delete[] tmp;
  return size * nmemb;
}

static std::wstring getNpmVersion(std::string nodeVersion) {
  nodeVersion = std::string("v") + nodeVersion;
  std::string res = "";
  CURL* curl = curl_easy_init();
  struct curl_slist* headers = nullptr;

  headers = curl_slist_append(headers, "Accept: */*");
  headers = curl_slist_append(headers, "User-Agent: Node.js Version Manager");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt(curl, CURLOPT_URL, "https://nodejs.org/dist/index.json");

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &onResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

  CURLcode code = curl_easy_perform(curl);

  if (code != CURLE_OK) {
    printf("%s\n", curl_easy_strerror(code));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return L"0.0.0";
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != "") {
    auto json = nlohmann::json::parse(res);
    unsigned int size = json.size();
    for (unsigned int i = 0; i < size; i++) {
      auto v = json[i]["version"].get<std::string>();
      if (v == nodeVersion) {
        return Util::a2w(json[i]["npm"].get<std::string>());
      }
    }
  }

  return L"0.0.0";
}

static bool uninstall(const std::wstring& version, const Config& config) {
  std::wstring nodeName = std::wstring(L"node-v") + version + L"-win32-" + config.arch + L".exe";
  std::wstring p = Path::join(config.root, nodeName);
  return Path::remove(p);
}

static bool install(const std::wstring& version, const Config& config) {
  std::wstring nodeName = std::wstring(L"node-v") + version + L"-win32-" + config.arch + L".exe";
  std::wstring exePath = Path::join(config.root, nodeName);

  if (Path::exists(exePath)) {
    printf("Version %s (%s) is already installed.\n", Util::w2a(version).c_str(), Util::w2a(config.arch).c_str());
    return true;
  }

  bool res = true;

  if (!Path::exists(exePath)) {
    ProgressBar* prog = new ProgressBar(std::wstring(L"Downloading ") + nodeName, 0, 100, 0, 0);
    if (config.node_mirror == L"default") {
      res = download(
        std::wstring(L"https://nodejs.org/dist/v") + version + L"/win-" + config.arch + L"/node.exe",
        exePath,
        onDownload,
        prog
      );
    }
    else if (config.node_mirror == L"taobao") {
      res = download(
        std::wstring(L"https://npm.taobao.org/mirrors/node/v") + version + L"/win-" + config.arch + L"/node.exe",
        exePath,
        onDownload,
        prog
      );
    }
    else {
      res = download(
        config.node_mirror + L"v" + version + L"/win-" + config.arch + L"/node.exe",
        exePath,
        onDownload,
        prog
      );
    }
    delete prog;
  }

  if (!res) return false;

  return res;
}

static void list(const Config& config) {
  auto li = Path::readdir(config.root);

  printf("\n");

  std::wstring wver = L"0.0.0";
  std::wstring nodeexe = Path::join(Path::__dirname(), L"node.exe");
  std::wstring tmpversion = Path::join(Path::__dirname(), L"version");
  std::wstring currentArch = L"";
  if (Path::exists(nodeexe)) {
    system((Util::w2a(nodeexe) + " -v>\"" + Util::w2a(tmpversion) + "\"").c_str());
    std::string ver = Path::readFile(tmpversion).substr(1);
    if (ver != "") {
      ver.erase(ver.find("\r\n"));
      Path::remove(tmpversion);
    }
    wver = Util::a2w(ver);
    currentArch = Util::isX64(nodeexe) ? L"x64" : L"x86";
  }

  for (unsigned int i = 0; i < li.size(); i++) {
    auto targetFile = Path::join(config.root, li[i]);
    if (!Path::isDirectory(targetFile) && Path::extname(targetFile) == L".exe") {
      std::wstring nodever = li[i].substr(li[i].find(L"-v") + 2).substr(0, li[i].substr(li[i].find(L"-v") + 2).find(L"-"));
      std::wstring nodearch = li[i].substr(li[i].find(L"-x") + 1, 3);
      if ((wver == nodever || wver == std::wstring(L"v") + nodever) && currentArch == nodearch) {
        printf("  * %s - %s\n", Util::w2a(wver).c_str(), Util::w2a(currentArch).c_str());
      } else {
        printf("    %s - %s\n", Util::w2a(nodever).c_str(), Util::w2a(nodearch).c_str());
      }
    }
  }
}

static bool uninstallNpm() {
  std::wstring __dirname = Path::__dirname();
  std::wstring npmdir = Path::join(__dirname, L"node_modules", L"npm");
  std::wstring npm = Path::join(__dirname, L"npm");
  std::wstring npmcmd = Path::join(__dirname, L"npm.cmd");
  std::wstring npx = Path::join(__dirname, L"npx");
  std::wstring npxcmd = Path::join(__dirname, L"npx.cmd");

  if (Path::isDirectory(npmdir)) {
    if (!Path::remove(npmdir)) return false;
  }

  if (Path::exists(npm)) {
    if (!Path::remove(npm)) return false;
  }

  if (Path::exists(npmcmd)) {
    if (!Path::remove(npmcmd)) return false;
  }

  if (Path::exists(npx)) {
    if (!Path::remove(npx)) return false;
  }

  if (Path::exists(npxcmd)) {
    if (!Path::remove(npxcmd)) return false;
  }

  return true;
}

static bool usenpm(const std::wstring& npmver, const Config& config) {
  std::wstring __dirname = Path::__dirname();
  std::wstring npmdir = Path::join(__dirname, L"node_modules", L"npm");
  std::wstring packagejson = Path::join(npmdir, L"package.json");
  std::wstring npm = Path::join(__dirname, L"npm");
  std::wstring npmcmd = Path::join(__dirname, L"npm.cmd");
  std::wstring npx = Path::join(__dirname, L"npx");
  std::wstring npxcmd = Path::join(__dirname, L"npx.cmd");

  if (Path::exists(packagejson) && Path::exists(npmcmd)) {
    nlohmann::json pkg = nlohmann::json::parse(Path::readFile(packagejson));
    auto version = pkg["version"].get<std::string>();
    if (version == Util::w2a(npmver)) {
      printf("Now using npm v%s\n", Util::w2a(npmver).c_str());
      return true;
    }
  }

  std::wstring zipPath = Path::join(config.cache, npmver + L".zip");

  bool res = true;
  if (!Path::exists(zipPath)) {
    ProgressBar* prog = new ProgressBar(std::wstring(L"Downloading npm v") + npmver, 0, 100, 0, 0);
    if (config.npm_mirror == L"default") {
      res = download(
        std::wstring(L"https://github.com/npm/cli/archive/v") + npmver + L".zip",
        zipPath,
        onDownload,
        prog
      );
    } else if (config.npm_mirror == L"taobao") {
      res = download(
        std::wstring(L"https://npm.taobao.org/mirrors/npm/v") + npmver + L".zip",
        zipPath,
        onDownload,
        prog
      );
    } else {
      res = download(
        config.npm_mirror + L"v" + npmver + L".zip",
        zipPath,
        onDownload,
        prog
      );
    }
    delete prog;

    if (!res) return false;
  }

  if (!Path::exists(zipPath)) return false;

  if (!uninstallNpm()) return false;

  ProgressBar* prog = new ProgressBar(std::wstring(L"Extracting npm ") + npmver, 0, 100, 0, 0);
  unzipparam unzipp;
  unzipp.prog = prog;
  unzipp.rootname = L"";
  res = Util::unzip(zipPath, Path::join(Path::__dirname(), L"node_modules"), onUnzip, &unzipp);
  delete prog;

  if (!res) return false;

  res = Path::rename(Path::join(Path::__dirname(), L"node_modules", unzipp.rootname), npmdir);

  if (!res) return false;

  if (!Path::exists(npmcmd)) {
    Path::copyFile(Path::join(npmdir, L"bin", L"npm"), npm);
    res = Path::copyFile(Path::join(npmdir, L"bin", L"npm.cmd"), npmcmd);
    Path::copyFile(Path::join(npmdir, L"bin", L"npx"), npx);
    Path::copyFile(Path::join(npmdir, L"bin", L"npx.cmd"), npxcmd);
  }

  if (!res) return false;

  printf("Now using npm v%s\n", Util::w2a(npmver).c_str());
  return true;
}

static bool use(const std::wstring& version, const Config& config) {
  bool res = true;
  std::wstring nodeName = std::wstring(L"node-v") + version + L"-win32-" + config.arch + L".exe";
  std::wstring exePath = Path::join(config.root, nodeName);
  std::wstring npmroot = Path::join(Path::__dirname(), L"node_modules", L"npm");
  std::wstring npmpackagejson = Path::join(npmroot, L"package.json");
  std::wstring npmcmd = Path::join(Path::__dirname(), L"npm.cmd");

  if (!Path::exists(exePath)) {
    if (!install(version, config)) return false;
  }

  if (!Path::exists(npmpackagejson)) {
    std::wstring npmver = getNpmVersion(Util::w2a(version));
    if (npmver == L"0.0.0") return false;
    if (!usenpm(npmver, config)) return false;
  }

  if (!Path::copyFile(exePath, Path::join(Path::__dirname(), L"node.exe"))) return false;

  printf("Now using Node.js v%s (%s)\n", Util::w2a(version).c_str(), Util::w2a(config.arch).c_str());
  return true;
}

static void printHelp() {
  printf("\n");
  printf("Node.js Version Manager %s\n\n", NVM_VERSION);

  printf("Usage:\n\n");
  printf("  nvm version\n");
  printf("  nvm arch [x86 | x64]\n");
  printf("  nvm cache [<npm cache dir>]\n");
  printf("  nvm root [<node binary dir>]\n");
  printf("  nvm list\n");
  printf("  nvm use <node version> [options]\n");
  printf("  nvm usenpm <npm version> [options]\n");
  printf("  nvm uninstall <node version>\n");
  printf("  nvm rmnpm\n");
  printf("  nvm install <node version> [options]\n");
  printf("  nvm node_mirror [default | taobao | <url>]\n");
  printf("  nvm npm_mirror [default | taobao | <url>]\n");

  printf("\n");

  printf("Options:\n\n");
  printf("  --arch=<x86 | x64>\n");
  printf("  --node_mirror=<default | taobao | <url>>\n");
  printf("  --npm_mirror=<default | taobao | <url>>\n");
  printf("  --cache=<npm cache dir>\n");
  printf("  --root=<node binary dir>\n");
}

int wmain(int argc, wchar_t** argv) {
  Cli cli(argc, argv);
  Config config(&cli);

  std::wstring command = cli.getCommand();
  if (command == L"version" || command == L"v" || command == L"-v") {
    printf("%s\n", NVM_VERSION);
    return 0;
  }

  if (command == L"arch") {
    if (cli.getArgument().size() == 0) {
      printf("%s\n", Util::w2a(config.arch).c_str());
      return 0;
    }
    
    config.set(L"arch", cli.getArgument()[0]);
    return 0;
  }

  if (command == L"node_mirror") {
    if (cli.getArgument().size() == 0) {
      printf("%s\n", Util::w2a(config.node_mirror).c_str());
      return 0;
    }

    config.set(L"node_mirror", cli.getArgument()[0]);
    return 0;
  }

  if (command == L"npm_mirror") {
    if (cli.getArgument().size() == 0) {
      printf("%s\n", Util::w2a(config.npm_mirror).c_str());
      return 0;
    }

    config.set(L"npm_mirror", cli.getArgument()[0]);
    return 0;
  }

  if (command == L"cache") {
    if (cli.getArgument().size() == 0) {
      printf("%s\n", Util::w2a(config.cache).c_str());
      return 0;
    }

    config.set(L"cache", cli.getArgument()[0]);
    return 0;
  }

  if (command == L"root") {
    if (cli.getArgument().size() == 0) {
      printf("%s\n", Util::w2a(config.root).c_str());
      return 0;
    }

    config.set(L"root", cli.getArgument()[0]);
    return 0;
  }

  if (command == L"install") {
    if (cli.getArgument().size() == 0) {
      printf("Example: \n\n  nvm.exe install 4.1.4\n");
      return 0;
    }

    install(cli.getArgument()[0], config);
    return 0;
  }

  if (command == L"list") {
    list(config);
    return 0;
  }

  if (command == L"use") {
    if (cli.getArgument().size() == 0) {
      printf("Example: \n\n  nvm.exe use 4.1.4\n");
      return 0;
    }

    if (!use(cli.getArgument()[0], config)) {
      printf("Use failed.\n");
    }
    return 0;
  }

  if (command == L"uninstall") {
    if (cli.getArgument().size() == 0) {
      printf("Example: \n\n  nvm.exe uninstall 4.1.4\n");
      return 0;
    }

    if (!uninstall(cli.getArgument()[0], config)) {
      printf("Uninstall failed.\n");
    }
    return 0;
  }

  if (command == L"usenpm") {
    if (cli.getArgument().size() == 0) {
      printf("Example: \n\n  nvm.exe usenpm 6.9.0\n");
      return 0;
    }

    if (!usenpm(cli.getArgument()[0], config)) {
      printf("usenpm failed.\n");
    }
    return 0;
  }

  if (command == L"rmnpm") {
    uninstallNpm();
    return 0;
  }

  printHelp();
  return 0;
}
