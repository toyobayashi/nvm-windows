#ifndef __INCLUDE_CONFIG_H__
#define __INCLUDE_CONFIG_H__

#include <string>
#include "cli.h"

class Config {
private:
  Config();
public:
  Config(Cli* cli = nullptr);
  std::wstring arch;
  std::wstring node_mirror;
  std::wstring npm_mirror;
  std::wstring cache;
  std::wstring root;

  void set(const std::wstring& key, const std::wstring& value);
};

#endif
