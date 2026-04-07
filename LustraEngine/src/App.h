#pragma once

#include "Window.h"

#include <cstdint>

class App
{
  public:
	App(const char* appName);
	~App();

	void CreateWindow(const char* name, uint32_t width, uint32_t height);
	// Returns if quit with error or not.
	bool RunApp();

  private:
	const char* m_name = nullptr;
	Window m_window;
};
