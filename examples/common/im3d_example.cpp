#include "im3d_example.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Im3d;

#ifdef IM3D_COMPILER_MSVC
	#pragma warning(disable: 4996) // vsnprintf
#endif

/******************************************************************************/
#if defined(IM3D_PLATFORM_WIN)
	static LARGE_INTEGER g_SysTimerFreq;

	const char* Im3d::GetWinErrorString(DWORD _err)
	{
		const int kErrMsgMax = 1024;
		static char buf[kErrMsgMax];
		buf[0] = '\0';
		IM3D_VERIFY(
			FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
				NULL, 
				_err,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)buf, 
				kErrMsgMax, 
				NULL
			) != 0
		);
		return buf;
	}
	
	static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _umsg, WPARAM _wparam, LPARAM _lparam)
	{
		ImGuiIO& imgui = ImGui::GetIO();
		Example* im3d = g_Example;
		
		switch (_umsg) {
		case WM_SIZE: {
			int w = (int)LOWORD(_lparam), h = (int)HIWORD(_lparam);
			if (im3d->m_width != w || im3d->m_height != h) {
				im3d->m_width = w;
				im3d->m_height = h;
			}
			break;
		}
		case WM_SIZING: {
			RECT* r = (RECT*)_lparam;
			int w = (int)(r->right - r->left);
			int h = (int)(r->bottom - r->top);
			if (im3d->m_width != w || im3d->m_height != h) {
				im3d->m_width = w;
				im3d->m_height = h;
			}
			break;
		}
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			imgui.MouseDown[0] = _umsg == WM_LBUTTONDOWN;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			imgui.MouseDown[2] = _umsg == WM_MBUTTONDOWN;
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			imgui.MouseDown[1] = _umsg == WM_RBUTTONDOWN;
			break;
		case WM_MOUSEWHEEL:
			imgui.MouseWheel = (float)(GET_WHEEL_DELTA_WPARAM(_wparam)) / (float)(WHEEL_DELTA); 
			break;
		case WM_MOUSEMOVE:
			io.MousePos.x = GET_X_LPARAM(_lparam);
			io.MousePos.y = GET_Y_LPARAM(_lparam);
			break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			WPARAM vk = _wparam;
			UINT sc = (_lparam & 0x00ff0000) >> 16;
			bool e0 = (_lparam & 0x01000000) != 0;
			if (vk == VK_SHIFT) {
				vk = MapVirtualKey(sc, MAPVK_VSC_TO_VK_EX);
			}
			switch (vk) {
			case VK_CONTROL:
				imgui.KeyCtrl = _umsg == WM_KEYDOWN;
				break;
			case VK_MENU:
				imgui.KeyAlt = _umsg == WM_KEYDOWN;
				break;
			case VK_LSHIFT:
			case VK_RSHIFT:
				imgui.KeyShift = _umsg == WM_KEYDOWN;
				break;
			case VK_ESCAPE:
				PostQuitMessage(0);
				break;
			default:
				if (vk < 512) {
					imgui.KeysDown[vk] = _umsg == WM_KEYDOWN;
				}
				break;
			};
			return 0;
		}
		case WM_CHAR:
			if (_wparam > 0 && _wparam < 0x10000) {
				imgui.AddInputCharacter((unsigned short)_wparam);
			}
			return 0;
		case WM_PAINT:
			//IM3D_ASSERT(false); // should be suppressed by calling ValidateRect()
			break;
		case WM_CLOSE:
			PostQuitMessage(0);
			return 0; // prevent DefWindowProc from destroying the window
		default:
			break;
		};
		return DefWindowProc(_hwnd, _umsg, _wparam, _lparam);
	}
	
	static bool InitWindow(int& _width_, int& _height_, const char* _title)
	{
		static ATOM wndclassex = 0;
		if (wndclassex == 0) {
			WNDCLASSEX wc;
			memset(&wc, 0, sizeof(wc));
			wc.cbSize = sizeof(wc);
			wc.style = CS_OWNDC;// | CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc = Impl::WindowProc;
			wc.hInstance = GetModuleHandle(0);
			wc.lpszClassName = "Im3dTestApp";
			wc.hCursor = LoadCursor(0, IDC_ARROW);
			winAssert(wndclassex = RegisterClassEx(&wc));
		}
	
		DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	
		if (_width_ == -1 || _height_ == -1) {
		// auto size; get the dimensions of the primary screen area and subtract the non-client area
			RECT r;
			winAssert(SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0));
			_width_  = r.right - r.left;
			_height_ = r.bottom - r.top;
	
			RECT wr = {};
			winAssert(AdjustWindowRectEx(&wr, dwStyle, FALSE, dwExStyle));
			_width_  -= wr.right - wr.left;
			_height_ -= wr.bottom - wr.top;
		}
	
		RECT r; r.top = 0; r.left = 0; r.bottom = _height_; r.right = _width_;
		winAssert(AdjustWindowRectEx(&r, dwStyle, FALSE, dwExStyle));
		g_Example->m_hwnd = CreateWindowEx(
			dwExStyle, 
			MAKEINTATOM(wndclassex), 
			_title, 
			dwStyle, 
			0, 0, 
			r.right - r.left, r.bottom - r.top, 
			NULL, 
			NULL, 
			GetModuleHandle(0), 
			NULL
			);
		IM3D_ASSERT(g_Example->m_hwnd);
		return true;
	}
	
	static void ShutdownWindow()
	{
		if (g_Example->m_hwnd) {
			winAssert(DestroyWindow(g_Example->m_hwnd);
		}
	}
	
	#if defined(IM3D_GFX_OPENGL)
		#include "GL/wglew.h"
		static PFNWGLCHOOSEPIXELFORMATARBPROC    wglChoosePixelFormat    = 0;
		static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs = 0;

		
		static bool InitOpenGL(int _vmaj, int _vmin)
		{
		 // create dummy context for extension loading
			static ATOM wndclassex = 0;
			if (wndclassex == 0) {
				WNDCLASSEX wc;
				memset(&wc, 0, sizeof(wc));
				wc.cbSize = sizeof(wc);
				wc.style = CS_OWNDC;// | CS_HREDRAW | CS_VREDRAW;
				wc.lpfnWndProc = DefWindowProc;
				wc.hInstance = GetModuleHandle(0);
				wc.lpszClassName = "Im3dTestApp_GlDummy";
				wc.hCursor = LoadCursor(0, IDC_ARROW);
				winAssert(wndclassex = RegisterClassEx(&wc));
			}
			HWND hwndDummy = CreateWindowEx(0, MAKEINTATOM(wndclassex), 0, NULL, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(0), NULL);
			IM3D_PLATFORM_ASSERT(hwndDummy);
			HDC hdcDummy = 0;
			winAssert(hdcDummy = GetDC(hwndDummy));	
			PIXELFORMATDESCRIPTOR pfd;
			memset(&pfd, 0, sizeof(pfd));
			winAssert(SetPixelFormat(hdcDummy, 1, &pfd));
			HGLRC hglrcDummy = 0;
			winAssert(hglrcDummy = wglCreateContext(hdcDummy));
			winAssert(wglMakeCurrent(hdcDummy, hglrcDummy));
			
		// check the platform supports the requested GL version
			GLint platformVMaj, platformVMin;
			glAssert(glGetIntegerv(GL_MAJOR_VERSION, &platformVMaj));
			glAssert(glGetIntegerv(GL_MINOR_VERSION, &platformVMin));
			if (platformVMaj < _vmaj || (platformVMaj >= _vmaj && platformVMin < _vmin)) {
				fprintf(stderr, "OpenGL version %d.%d is not available (max version is %d.%d)\n", _vmaj, _vmin, platformVMaj, platformVMin);
				fprintf(stderr, "\t(This error may occur if the platform has an integrated GPU)\n");
				return false;
			}
		
		// load wgl extensions for true context creation
			winAssert(wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB"));
			winAssert(wglCreateContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB"));
		
		// delete the dummy context
			winAssert(wglMakeCurrent(0, 0));
			winAssert(wglDeleteContext(hglrcDummy));
			winAssert(ReleaseDC(hwndDummy, hdcDummy) != 0);
			winAssert(DestroyWindow(hwndDummy) != 0);
		
		// create true context
			winAssert(m_hdc = GetDC(m_hwnd));
			const int pfattr[] = {
				WGL_SUPPORT_OPENGL_ARB, 1,
				WGL_DRAW_TO_WINDOW_ARB, 1,
				WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
				WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
				WGL_DOUBLE_BUFFER_ARB,  1,
				WGL_COLOR_BITS_ARB,     24,
				WGL_ALPHA_BITS_ARB,     8,
				WGL_DEPTH_BITS_ARB,     0,
				WGL_STENCIL_BITS_ARB,   0,
				0
			};
			int pformat = -1, npformat = -1;
			winAssert(wglChoosePixelFormat(m_hdc, pfattr, 0, 1, &pformat, (::UINT*)&npformat));
			winAssert(SetPixelFormat(m_hdc, pformat, &pfd));
			int profileBit = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
			//profileBit = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
			int attr[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB,	_vmaj,
				WGL_CONTEXT_MINOR_VERSION_ARB,	_vmin,
				WGL_CONTEXT_PROFILE_MASK_ARB,	profileBit,
				0
			};
			winAssert(m_hglrc = wglCreateContextAttribs(m_hdc, 0, attr));
		
		// load extensions
			if (!wglMakeCurrent(m_hdc, m_hglrc)) {
				fprintf(stderr, "wglMakeCurrent failed");
				return false;
			}
			glewExperimental = GL_TRUE;
			GLenum err = glewInit();
			IM3D_ASSERT(err == GLEW_OK);
			glGetError(); // clear any errors caused by glewInit()
		
			fprintf(stdout, "OpenGL context:\n\tVersion: %s\n\tGLSL Version: %s\n\tVendor: %s\n\tRenderer: %s\n",
				GlGetString(GL_VERSION),
				GlGetString(GL_SHADING_LANGUAGE_VERSION),
				GlGetString(GL_VENDOR),
				GlGetString(GL_RENDERER)
				);
			return true;
		}
		
		static void ShutdownOpenGL()
		{
			winAssert(wglMakeCurrent(0, 0));
			winAssert(wglDeleteContext(g_Example->m_hglrc));
			winAssert(ReleaseDC(g_Example->m_hwnd, g_Example->m_hdc) != 0);
		}
		
	#endif // graphics
	
#endif // platform

/******************************************************************************/
#if defined(IM3D_OPENGL)
	static void Append(const char* _str, Vector<char>& _out_)
	{
		while (*_str) {
			_out_.push_back(*_str);
			++_str;
		}
	}
	static void AppendLine(const char* _str, Vector<char>& _out_)
	{
		Append(_str, _out_);
		_out_.push_back('\n');
	}
	
	GLuint Im3d::LoadCompileShader(GLenum _stage, const char* _path, const char* _defines = 0)
	{
		Vector<char> src;
		AppendLine(IM3D_OPENGL_VSHADER, src);
		if (_defines) {
			while (*_defines != '\0') {
				Append("#define ", src);
				AppendLine(_defines, src);
				_defines = strchr(_defines, 0);
				IM3D_ASSERT(_defines);
				++_defines;
			}
		}
	
		FILE* fin = fopen(_path, "rb");
		if (!fin) {
			fprintf(stderr, "Error opening '%s'\n", _path);
			return 0;
		}
		IM3D_VERIFY(fseek(fin, 0, SEEK_END) == 0); // not portable but should work almost everywhere
		long fsize = ftell(fin);
		IM3D_VERIFY(fseek(fin, 0, SEEK_SET) == 0);
	
		int srcbeg = src.size();
		src.resize(srcbeg + fsize, '\0');
		if (fread(src.data() + srcbeg, 1, fsize, fin) != fsize) {
			fclose(fin);
			fprintf(stderr, "Error reading '%s'\n", _path);
			return 0;
		}
		fclose(fin);
		src.push_back('\0');
	
		GLuint ret = 0;
		glAssert(ret = glCreateShader(_stage));
		const GLchar* pd = src.data();
		GLint ps = src.size();
		glAssert(glShaderSource(ret, 1, &pd, &ps));
	
		glAssert(glCompileShader(ret));
		GLint compileStatus = GL_FALSE;
		glAssert(glGetShaderiv(ret, GL_COMPILE_STATUS, &compileStatus));
		if (compileStatus == GL_FALSE) {
			fprintf(stderr, "Error compiling '%s':\n\n", _path);
			GLint len;
			glAssert(glGetShaderiv(ret, GL_INFO_LOG_LENGTH, &len));
			char* log = new GLchar[len];
			glAssert(glGetShaderInfoLog(ret, len, 0, log));
			fprintf(stderr, log);
			delete[] log;
	
			fprintf(stderr, "\n\n%s", src.data());
			fprintf(stderr, "\n");
			glAssert(glDeleteShader(ret));
			return 0;
		}
		return ret;
	}
	
	bool Im3d::LinkShaderProgram(GLuint _handle)
	{
		IM3D_ASSERT(_handle != 0);
	
		glAssert(glLinkProgram(_handle));
		GLint linkStatus = GL_FALSE;
		glAssert(glGetProgramiv(_handle, GL_LINK_STATUS, &linkStatus));
		if (linkStatus == GL_FALSE) {
			fprintf(stderr, "Error linking program:\n\n");
			GLint len;
			glAssert(glGetProgramiv(_handle, GL_INFO_LOG_LENGTH, &len));
			GLchar* log = new GLchar[len];
			glAssert(glGetProgramInfoLog(_handle, len, 0, log));
			fprintf(stderr, log);
			fprintf(stderr, "\n");
			delete[] log;
	
			return false;
		}
		return true;
	}
	
	const char* Im3d::GetGlEnumString(GLenum _enum)
	{
		#define CASE_ENUM(e) case e: return #e
		switch (_enum) {
		// errors
			CASE_ENUM(GL_NONE);
			CASE_ENUM(GL_INVALID_ENUM);
			CASE_ENUM(GL_INVALID_VALUE);
			CASE_ENUM(GL_INVALID_OPERATION);
			CASE_ENUM(GL_INVALID_FRAMEBUFFER_OPERATION);
			CASE_ENUM(GL_OUT_OF_MEMORY);
	
			default: return "Unknown GLenum";
		};
		#undef CASE_ENUM
	}
	
	const char* Im3d::GlGetString(GLenum _name)
	{
		const char* ret;
		glAssert(ret = (const char*)glGetString(_name));
		return ret ? ret : "";
	}
	
#endif // graphics

/******************************************************************************/
static const char* StripPath(const char* _path) 
{
	int i = 0, last = 0;
	while (_path[i] != '\0') {
		if (_path[i] == '\\' || _path[i] == '/') {
			last = i + 1;
		}
		++i;
	}
	return &_path[last];
}

void Im3d::Assert(const char* _e, const char* _file, int _line, const char* _msg, ...)
{
	const int kAssertMsgMax = 1024;

	char buf[kAssertMsgMax];
	if (_msg != nullptr) {
		va_list args;
		va_start(args, _msg);
		vsnprintf(buf, kAssertMsgMax, _msg, args);
		va_end(args);
	} else {
		buf[0] = '\0';
	}
	fprintf(stderr, "Assert (%s, line %d)\n\t'%s' %s", StripPath(_file), _line, _e ? _e : "", buf);
}

void Im3d::RandSeed(int _seed)
{
	srand(_seed);
}
int Im3d::RandInt(int _min, int _max)
{
	return _min + (int)rand() % (_max - _min);
}
float Im3d::RandFloat(float _min, float _max)
{
	return _min + (float)rand() / (float)RAND_MAX * (_max - _min);
}
Vec3 Im3d::RandVec3(float _min, float _max)
{
	return Im3d::Vec3(
		RandFloat(_min, _max),
		RandFloat(_min, _max),
		RandFloat(_min, _max)
		);
}

/******************************************************************************/
static Example* Example::g_Example;

bool Example::init(int _width, int _height, const char* _title)
{
	g_Example = this;

	m_width  = _width;
	m_height = _height;
	m_title  = _title;
	if (!InitWindow(m_width, m_height, m_title)) {
		shutdown();
		return false;
	}
	
	#if defined(IM3D_PLATFORM_WIN)
	 // force the current working directory to the exe location
		TCHAR buf[MAX_PATH] = {};
		DWORD buflen;
		winAssert(buflen = GetModuleFileName(0, buf, MAX_PATH));
		char* pathend = strrchr(buf, (int)'\\');
		*(++pathend) = '\0';
		winAssert(SetCurrentDirectory(buf));
		fprintf(stdout, "Set current directory: '%s'\n", buf);
		
		winAssert(QueryPerformanceFrequency(&s_SysTimerFreq));
		winAssert(QueryPerformanceCounter(&m_currTime));
	#endif
	
	#if defined(IM3D_OPENGL) 
		if (!InitOpenGL(IM3D_OPENGL_VMAJ, IM3D_OPENGL_VMIN)) {
			shutdown();
			return false;
		}
	#endif
	
	return true;
}

void Example::shutdown()
{
	#if defined(IM3D_OPENGL) 
		ShutdownOpenGL();
	#endif
	
	ShutdownWindow();
}

bool Example::update()
{
	bool ret = true;
	#if defined(IM3D_PLATFORM_WIN)
		m_impl->m_prevTime = m_impl->m_currTime;
		winAssert(QueryPerformanceCounter(&m_currTime));
		double microseconds = (double)((m_impl->m_currTime.QuadPart - m_impl->m_prevTime.QuadPart) * 1000000ll / g_SysTimerFreq.QuadPart);
		m_deltaTime = (float)(microseconds / 1000000.0);
	
		MSG msg;
		while (PeekMessage(&msg, (HWND)m_impl->m_hwnd, 0, 0, PM_REMOVE) && msg.message != WM_QUIT) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		ret = msg != WM_QUIT;
	#endif
	
	return ret;
}

void Example::draw()
{
	#if defined(IM3D_PLATFORM_WIN)
		#if defined(IM3D_OPENGL)
			winAssert(SwapBuffers(m_hdc));
			winAssert(ValidateRect(m_hwnd, 0)); // suppress WM_PAINT
		#endif
	#endif
	
 // reset state & clear backbuffer for next frame
	#if defined(IM3D_OPENGL)
		glAssert(glBindVertexArray(0));
		glAssert(glUseProgram(0));
		glAssert(glViewport(0, 0, m_width, m_height));
		glAssert(glClearColor(0.2f, 0.2f, 0.2f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT));
	#endif
}

bool Example::hasFocus() const
{
	#if defined(IM3D_PLATFORM_WIN)
		return m_hwnd == GetFocus();
	#endif
}

Vec2 Example::getWindowRelativeCursor() const
{
	#if defined(IM3D_PLATFORM_WIN)
		POINT p = {};
		WinAssert(GetCursorPos(&p));
		WinAssert(ScreenToClient(m_hwnd, &p));
		return Vec2((float)p.x, (float)p.y);
	#endif
}
