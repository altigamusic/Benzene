#pragma once
#include <string>

struct DebugWindow
{
    void open(const std::string& error);
    void close();
    void render();

  private:
    bool show = false;
    std::string error;
};
