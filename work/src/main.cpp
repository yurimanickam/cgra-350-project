
// std
#include <iostream>
#include <string>
#include <stdexcept>

// project
#include "application.hpp"
#include "opengl.hpp"
#include "cgra/cgra_gui.hpp"


using namespace std;
using namespace cgra;


// forward decleration for cleanliness
namespace {
	void cursorPosCallback(GLFWwindow *, double xpos, double ypos);
	void mouseButtonCallback(GLFWwindow *win, int button, int action, int mods);
	void scrollCallback(GLFWwindow *win, double xoffset, double yoffset);
	void keyCallback(GLFWwindow *win, int key, int scancode, int action, int mods);
	void charCallback(GLFWwindow *win, unsigned int c);
	void APIENTRY debugCallback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);

	// global static pointer to application once we create it
	// nessesary for interfacing with the GLFW callbacks
	Application *application_ptr = nullptr;
}


// main program
// 
int main() {

	// initialize the GLFW library
	if (!glfwInit()) {
		cerr << "Error: Could not initialize GLFW" << endl;
		abort(); // unrecoverable error
	}

	// force OpenGL to create a 3.3 core context
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// disallow legacy functionality (helps OS X work)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// get the version for GLFW for later
	int glfwMajor, glfwMinor, glfwRevision;
	glfwGetVersion(&glfwMajor, &glfwMinor, &glfwRevision);

	// request a debug context so we get debug callbacks
	// remove this for possible GL performance increases
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

	// create a windowed mode window and its OpenGL context
	GLFWwindow *window = glfwCreateWindow(800, 600, "Hello World!", nullptr, nullptr);
	if (!window) {
		cerr << "Error: Could not create GLFW window" << endl;
		abort(); // unrecoverable error
	}

	// make the window's context current.
	// if we have multiple windows we will need to switch contexts
	glfwMakeContextCurrent(window);

	// initialize GLEW
	// must be done after making a GL context current (glfwMakeContextCurrent in this case)
	glewExperimental = GL_TRUE; // required for full GLEW functionality for OpenGL 3.0+
	GLenum err = glewInit();
	if (GLEW_OK != err) { // problem: glewInit failed, something is seriously wrong.
		cerr << "Error: " << glewGetErrorString(err) << endl;
		abort(); // unrecoverable error
	}

	// print out our OpenGL versions
	cout << "Using OpenGL " << glGetString(GL_VERSION) << endl;
	cout << "Using GLEW " << glewGetString(GLEW_VERSION) << endl;
	cout << "Using GLFW " << glfwMajor << "." << glfwMinor << "." << glfwRevision << endl;

	// enable GL_ARB_debug_output if available (not necessary, just helpful)
	if (glfwExtensionSupported("GL_ARB_debug_output")) {
		// this allows the error location to be determined from a stacktrace
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		// setup up the callback
		glDebugMessageCallbackARB(debugCallback, nullptr);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
		cout << "GL_ARB_debug_output callback installed" << endl;
	}
	else {
		cout << "GL_ARB_debug_output not available. No worries." << endl;
	}

	// initialize ImGui
	if (!cgra::gui::init(window)) {
		cerr << "Error: Could not initialize ImGui" << endl;
		abort(); // unrecoverable error
	}

	// attach input callbacks to window
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCharCallback(window, charCallback);

	
	// create the application object (and a global pointer to it)
	Application application(window);
	application_ptr = &application;

	// loop until the user closes the window
	while (!glfwWindowShouldClose(window)) {

		// main Render
		//glEnable(GL_FRAMEBUFFER_SRGB); // use if you know about gamma correction
		application.render();

		// GUI Render on top
		//glDisable(GL_FRAMEBUFFER_SRGB); // use if you know about gamma correction
		cgra::gui::newFrame();
		application.renderGUI();
		cgra::gui::render();

		// swap front and back buffers
		glfwSwapBuffers(window);

		// poll for and process events
		glfwPollEvents();
	}

	// clean up ImGui
	cgra::gui::shutdown();
	glfwTerminate();
}


namespace {

	void cursorPosCallback(GLFWwindow *, double xpos, double ypos) {
		// if not captured then foward to application
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) return;
		application_ptr->cursorPosCallback(xpos, ypos);
	}


	void mouseButtonCallback(GLFWwindow *win, int button, int action, int mods) {
		// forward callback to ImGui
		cgra::gui::mouseButtonCallback(win, button, action, mods);

		// if not captured then foward to application
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) return;
		application_ptr->mouseButtonCallback(button, action, mods);
	}


	void scrollCallback(GLFWwindow *win, double xoffset, double yoffset) {
		// forward callback to ImGui
		cgra::gui::scrollCallback(win, xoffset, yoffset);

		// if not captured then foward to application
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) return;
		application_ptr->scrollCallback(xoffset, yoffset);
	}


	void keyCallback(GLFWwindow *win, int key, int scancode, int action, int mods) {
		// forward callback to ImGui
		cgra::gui::keyCallback(win, key, scancode, action, mods);

		// if not captured then foward to application
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard) return;
		application_ptr->keyCallback(key, scancode, action, mods);
	}


	void charCallback(GLFWwindow *win, unsigned int c) {
		// forward callback to ImGui
		cgra::gui::charCallback(win, c);

		// if not captured then foward to application
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput) return;
		application_ptr->charCallback(c);
	}


	// function to translate source to string
	const char * getStringForSource(GLenum source) {
		switch (source) {
		case GL_DEBUG_SOURCE_API:
			return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
			return "Window System";
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			return "Shader Compiler";
		case GL_DEBUG_SOURCE_THIRD_PARTY:
			return "Third Party";
		case GL_DEBUG_SOURCE_APPLICATION:
			return "Application";
		case GL_DEBUG_SOURCE_OTHER:
			return "Other";
		default:
			return "n/a";
		}
	}

	// function to translate severity to string
	const char * getStringForSeverity(GLenum severity) {
		switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			return "High";
		case GL_DEBUG_SEVERITY_MEDIUM:
			return "Medium";
		case GL_DEBUG_SEVERITY_LOW:
			return "Low";
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			return "None";
		default:
			return "n/a";
		}
	}

	// function to translate type to string
	const char * getStringForType(GLenum type) {
		switch (type) {
		case GL_DEBUG_TYPE_ERROR:
			return "Error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			return "Deprecated Behaviour";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			return "Undefined Behaviour";
		case GL_DEBUG_TYPE_PORTABILITY:
			return "Portability";
		case GL_DEBUG_TYPE_PERFORMANCE:
			return "Performance";
		case GL_DEBUG_TYPE_OTHER:
			return "Other";
		default:
			return "n/a";
		}
	}

	// actually define the function
	void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* message, GLvoid*) {
		// Don't report notification messages
		if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

		// nvidia: avoid debug spam about attribute offsets
		if (id == 131076) return;

		cerr << "GL [" << getStringForSource(source) << "] " << getStringForType(type) << ' ' << id << " : ";
		cerr << message << " (Severity: " << getStringForSeverity(severity) << ')' << endl;

		if (type == GL_DEBUG_TYPE_ERROR_ARB) throw runtime_error("GL Error: "s + message);
	}
}