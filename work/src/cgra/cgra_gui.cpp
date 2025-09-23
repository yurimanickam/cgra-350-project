
// std
#include <iostream>

// project
#include "cgra_gui.hpp"


using namespace std;


namespace cgra {

	namespace {

		// internal data
		GLFWwindow*  g_window = NULL;
		double       g_time = 0.0f;
		bool         g_mousePressed[3] = { false, false, false };
		float        g_mouseWheel = 0.0f;
		GLuint       g_fontTexture = 0;
		int          g_shaderHandle = 0, g_vertHandle = 0, g_fragHandle = 0;
		int          g_attribLocationTex = 0, g_attribLocationProjMtx = 0;
		int          g_attribLocationPosition = 0, g_attribLocationUV = 0, g_attribLocationColor = 0;
		unsigned int g_vboHandle = 0, g_vaoHandle = 0, g_elementsHandle = 0;


		void createFontsTexture() {
			// build texture atlas
			ImGuiIO& io = ImGui::GetIO();
			unsigned char* pixels;
			int width, height;

			// Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small)
			// because it is more likely to be compatible with user's existing shaders.
			// If your ImTextureId represent a higher-level concept than just a GL texture id,
			// consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   

			// upload texture to graphics system
			GLint last_texture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			glGenTextures(1, &g_fontTexture);
			glBindTexture(GL_TEXTURE_2D, g_fontTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			// store our identifier
			io.Fonts->TexID = (void *)(intptr_t)g_fontTexture;

			// restore state
			glBindTexture(GL_TEXTURE_2D, last_texture);
		}

		bool createDeviceObjects() {
			// backup GL state
			GLint last_texture, last_array_buffer, last_vertex_array;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
			glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

			const GLchar *vertex_shader =
				"#version 330\n"
				"uniform mat4 uProjectionMatrix;\n"
				"in vec2 vPosition;\n"
				"in vec2 vUV;\n"
				"in vec4 vColor;\n"
				"out vec2 Frag_UV;\n"
				"out vec4 Frag_Color;\n"
				"void main() {\n"
				"   Frag_UV = vUV;\n"
				"   Frag_Color = vColor;\n"
				"   gl_Position = uProjectionMatrix * vec4(vPosition.xy,0,1);\n"
				"}\n";

			const GLchar* fragment_shader =
				"#version 330\n"
				"uniform sampler2D Texture;\n"
				"in vec2 Frag_UV;\n"
				"in vec4 Frag_Color;\n"
				"out vec4 Out_Color;\n"
				"void main() {\n"
				"   Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
				"}\n";

			g_shaderHandle = glCreateProgram();
			g_vertHandle = glCreateShader(GL_VERTEX_SHADER);
			g_fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(g_vertHandle, 1, &vertex_shader, 0);
			glShaderSource(g_fragHandle, 1, &fragment_shader, 0);
			glCompileShader(g_vertHandle);
			glCompileShader(g_fragHandle);
			glAttachShader(g_shaderHandle, g_vertHandle);
			glAttachShader(g_shaderHandle, g_fragHandle);
			glLinkProgram(g_shaderHandle);

			g_attribLocationTex = glGetUniformLocation(g_shaderHandle, "Texture");
			g_attribLocationProjMtx = glGetUniformLocation(g_shaderHandle, "uProjectionMatrix");
			g_attribLocationPosition = glGetAttribLocation(g_shaderHandle, "vPosition");
			g_attribLocationUV = glGetAttribLocation(g_shaderHandle, "vUV");
			g_attribLocationColor = glGetAttribLocation(g_shaderHandle, "vColor");

			glGenBuffers(1, &g_vboHandle);
			glGenBuffers(1, &g_elementsHandle);

			glGenVertexArrays(1, &g_vaoHandle);
			glBindVertexArray(g_vaoHandle);
			glBindBuffer(GL_ARRAY_BUFFER, g_vboHandle);
			glEnableVertexAttribArray(g_attribLocationPosition);
			glEnableVertexAttribArray(g_attribLocationUV);
			glEnableVertexAttribArray(g_attribLocationColor);

		#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
			glVertexAttribPointer(g_attribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
			glVertexAttribPointer(g_attribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
			glVertexAttribPointer(g_attribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
		#undef OFFSETOF

			createFontsTexture();

			// restore modified GL state
			glBindTexture(GL_TEXTURE_2D, last_texture);
			glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
			glBindVertexArray(last_vertex_array);

			return true;
		}


		void invalidateDeviceObjects() {
			if (g_vaoHandle) glDeleteVertexArrays(1, &g_vaoHandle);
			if (g_vboHandle) glDeleteBuffers(1, &g_vboHandle);
			if (g_elementsHandle) glDeleteBuffers(1, &g_elementsHandle);
			g_vaoHandle = g_vboHandle = g_elementsHandle = 0;

			if (g_shaderHandle && g_vertHandle) glDetachShader(g_shaderHandle, g_vertHandle);
			if (g_vertHandle) glDeleteShader(g_vertHandle);
			g_vertHandle = 0;

			if (g_shaderHandle && g_fragHandle) glDetachShader(g_shaderHandle, g_fragHandle);
			if (g_fragHandle) glDeleteShader(g_fragHandle);
			g_fragHandle = 0;

			if (g_shaderHandle) glDeleteProgram(g_shaderHandle);
			g_shaderHandle = 0;

			if (g_fontTexture) {
				glDeleteTextures(1, &g_fontTexture);
				ImGui::GetIO().Fonts->TexID = 0;
				g_fontTexture = 0;
			}
		}



		void renderDrawLists(ImDrawData* draw_data) {
			// avoid rendering when minimized, scale coordinates for
			// retina displays (screen coordinates != framebuffer coordinates)
			ImGuiIO& io = ImGui::GetIO();
			int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
			int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
			if (fb_width == 0 || fb_height == 0)
				return;
			draw_data->ScaleClipRects(io.DisplayFramebufferScale);

			// backup GL state
			GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
			glActiveTexture(GL_TEXTURE0);
			GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
			GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
			GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
			GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
			GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
			GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
			GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
			GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
			GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
			GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
			GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
			GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
			GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
			GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
			GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
			GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
			GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

			// setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_SCISSOR_TEST);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			// setup viewport, orthographic projection matrix
			glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
			const float ortho_projection[4][4] = {
				{ 2.0f / io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
				{ 0.0f,                  2.0f / -io.DisplaySize.y, 0.0f, 0.0f },
				{ 0.0f,                  0.0f,                  -1.0f, 0.0f },
				{ -1.0f,                  1.0f,                   0.0f, 1.0f },
			};
			glUseProgram(g_shaderHandle);
			glUniform1i(g_attribLocationTex, 0);
			glUniformMatrix4fv(g_attribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
			glBindVertexArray(g_vaoHandle);

			for (int n = 0; n < draw_data->CmdListsCount; n++) {
				const ImDrawList* cmd_list = draw_data->CmdLists[n];
				const ImDrawIdx* idx_buffer_offset = 0;

				glBindBuffer(GL_ARRAY_BUFFER, g_vboHandle);
				glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_elementsHandle);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

				for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
					if (pcmd->UserCallback) {
						pcmd->UserCallback(cmd_list, pcmd);
					}
					else {
						glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
						glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
						glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
					}
					idx_buffer_offset += pcmd->ElemCount;
				}
			}

			// restore modified GL state
			glUseProgram(last_program);
			glBindTexture(GL_TEXTURE_2D, last_texture);
			glActiveTexture(last_active_texture);
			glBindVertexArray(last_vertex_array);
			glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
			glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
			glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
			if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
			if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
			if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
			if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
			glPolygonMode(GL_FRONT_AND_BACK, last_polygon_mode[0]);
			glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
			glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
		}


		const char* getClipboardText(void* user_data) {
			return glfwGetClipboardString((GLFWwindow*)user_data);
		}


		void setClipboardText(void* user_data, const char* text) {
			glfwSetClipboardString((GLFWwindow*)user_data, text);
		}

	}

	namespace gui {

		void mouseButtonCallback(GLFWwindow*, int button, int action, int /*mods*/) {
			if (action == GLFW_PRESS && button >= 0 && button < 3)
				g_mousePressed[button] = true;
		}


		void scrollCallback(GLFWwindow*, double /*xoffset*/, double yoffset) {
			g_mouseWheel += (float)yoffset; // use fractional mouse wheel, 1.0 unit 5 lines.
		}


		void keyCallback(GLFWwindow*, int key, int /*scancode*/, int action, int mods) {
			ImGuiIO& io = ImGui::GetIO();
			if (action == GLFW_PRESS)
				io.KeysDown[key] = true;
			if (action == GLFW_RELEASE)
				io.KeysDown[key] = false;

			(void)mods; // modifiers are not reliable across systems
			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
			io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
		}

		void charCallback(GLFWwindow*, unsigned int c) {
			ImGuiIO& io = ImGui::GetIO();
			if (c > 0 && c < 0x10000)
				io.AddInputCharacter((unsigned short)c);
		}


		bool init(GLFWwindow* window, bool install_callbacks) {
			g_window = window;

			ImGuiIO& io = ImGui::GetIO();
			// keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
			io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
			io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
			io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
			io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
			io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
			io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
			io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
			io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
			io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
			io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
			io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
			io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
			io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
			io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
			io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
			io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
			io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
			io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
			io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

			// alternatively you can set this to NULL and call ImGui::GetDrawData()
			// after ImGui::Render() to get the same ImDrawData pointer.
			io.RenderDrawListsFn = renderDrawLists;
			io.SetClipboardTextFn = setClipboardText;
			io.GetClipboardTextFn = getClipboardText;
			io.ClipboardUserData = g_window;

			if (install_callbacks) {
				glfwSetMouseButtonCallback(window, mouseButtonCallback);
				glfwSetScrollCallback(window, scrollCallback);
				glfwSetKeyCallback(window, keyCallback);
				glfwSetCharCallback(window, charCallback);
			}

			return true;
		}

		void newFrame() {
			if (!g_fontTexture)
				createDeviceObjects();

			ImGuiIO& io = ImGui::GetIO();

			// setup display size (every frame to accommodate for window resizing)
			int w, h;
			int display_w, display_h;
			glfwGetWindowSize(g_window, &w, &h);
			glfwGetFramebufferSize(g_window, &display_w, &display_h);
			io.DisplaySize = ImVec2((float)w, (float)h);
			io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

			// setup time step
			double current_time =  glfwGetTime();
			io.DeltaTime = g_time > 0.0 ? (float)(current_time - g_time) : (float)(1.0f/60.0f);
			g_time = current_time;

			// setup inputs
			// we already got mouse wheel, keyboard keys & characters
			// from glfw callbackspolled in glfwPollEvents()
			if (glfwGetWindowAttrib(g_window, GLFW_FOCUSED)) {
				double mouse_x, mouse_y;
				glfwGetCursorPos(g_window, &mouse_x, &mouse_y);
				// Mouse position in screen coordinates
				// (set to -1,-1 if no mouse / on another screen, etc.)
				io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
			} else {
				io.MousePos = ImVec2(-1,-1);
			}

			for (int i = 0; i < 3; i++) {
				// If a mouse press event came, always pass it as "mouse held this frame",
				// so we don't miss click-release events that are shorter than 1 frame.
				io.MouseDown[i] = g_mousePressed[i] || glfwGetMouseButton(g_window, i) != 0;
				g_mousePressed[i] = false;
			}

			io.MouseWheel = g_mouseWheel;
			g_mouseWheel = 0.0f;

			// hide OS mouse cursor if ImGui is drawing it
			glfwSetInputMode(g_window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

			// start the frame
			ImGui::NewFrame();
		}

		void render() {
			ImGui::Render();
		}

		void shutdown() {
			invalidateDeviceObjects();
			ImGui::Shutdown();
		}

	}

}