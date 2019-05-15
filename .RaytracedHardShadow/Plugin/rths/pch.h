#pragma once

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgiformat.h>
#include <dxcapi.h>
#include <comdef.h>
#include "dxcapi.use.h"
#include "d3dx12.h"
#endif // _WIN32

// Unity PluginAPI
#include "IUnityInterface.h"
#include "IUnityGraphics.h"
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphicsD3D12.h"

#include <array>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <future>
#include <random>
#include <regex>
#include <iterator>
#include <locale>
#include <codecvt>
