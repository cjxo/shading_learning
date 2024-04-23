#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <math.h>

#include "s_base.h"
#include "s_math.h"

#include "s_base.c"
#include "s_math.c"

typedef struct {
	u32 client_width;
	u32 client_height;
	b32 resized_this_frame;
	HWND handle;
} OS_Window;

typedef u8 OS_Input_Flags;
enum {
	OSInput_Flag_Quit = (1 << 0),
};

typedef struct {
	OS_Input_Flags flags;
} OS_Input;

function s32
os_message_box(String_Const_U8 title, String_Const_U8 message) {
	s32 result = MessageBoxA(null, (char *)(message.str),
							 (char *)(title.str),
							 MB_OK);
	return(result);
}

function OS_Window
os_create_window(String_Const_U8 name, u32 width, u32 height) {
	OS_Window result;

	RECT window_rect;
	window_rect.left = 0;
	window_rect.top = 0;
	window_rect.right = width;
	window_rect.bottom = height;
	AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND window_handle = CreateWindowA("my_window_class",
									   (char *)name.str, WS_OVERLAPPEDWINDOW,
										0, 0, window_rect.right - window_rect.left,
										window_rect.bottom - window_rect.top, null,
										null, GetModuleHandleA(null), null);

	if (!IsWindow(window_handle)) {
		os_message_box(str8("Error"), str8("Failed To Create Window"));
		ExitProcess(1);
	}

	ShowWindow(window_handle, SW_SHOW);

	result.client_width = width;
	result.client_height = height;
	result.resized_this_frame = False;
	result.handle = window_handle;
	return(result);
}

function LRESULT __stdcall
w32_window_proc(HWND window, UINT message, 
				WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;

	OS_Window *os_window = (OS_Window *)GetWindowLongPtrA(window, GWLP_USERDATA);
	switch (message) {
		case WM_CLOSE: {
			DestroyWindow(window);
		} break;

		case WM_SIZE: {
			if (os_window) {
				u32 new_width = (u32)LOWORD(lparam);
				u32 new_height = (u32)HIWORD(lparam);

				os_window->client_width = new_width;
				os_window->client_height = new_height;
				os_window->resized_this_frame = False;
			}
		} break;

		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;

		default: {
			return DefWindowProc(window, message, wparam, lparam);
		} break;
	}

	return(result);
}

function void
os_fill_events(OS_Input *input, OS_Window *window) {
	window->resized_this_frame = False;
	SetWindowLongPtrA(window->handle, GWLP_USERDATA, (LONG_PTR)window);

	MSG message;
	while (PeekMessageA(&message, null, 0, 0, PM_REMOVE) != 0) {
		switch (message.message) {
			case WM_QUIT: {
				input->flags |= OSInput_Flag_Quit;
			} break;

			default: {
				TranslateMessage(&message);
				DispatchMessage(&message);
			} break;
		}
	}
}

typedef struct {
	ID3D11Device *base_device;
	ID3D11Device1 *main_device;
	ID3D11DeviceContext *base_device_context;
	IDXGISwapChain1 *swap_chain;
	ID3D11Texture2D *back_buffer;
	ID3D11RenderTargetView *back_buffer_as_rtv;
	ID3D11RasterizerState1 *fill_cull_raster;
	ID3D11RasterizerState1 *wire_nocull_raster;
	ID3D11RasterizerState1 *wire_cull_raster;
	ID3D11Texture2D *depth_buffer_texture;
	ID3D11DepthStencilView *depth_buffer_view;
	ID3D11DepthStencilState *depth_buffer_state;
} D3D11_State;

__declspec(align(16)) typedef struct {
	m44 perspective;
} D3D11_Constants;

typedef struct {
	v3f position;
	quat orient;
	v3f scale;
	v4f colour;
} Model_Instance;

function void
d3d11_create_swap_chain(D3D11_State *state, OS_Window *os_window) {
	IDXGIDevice *dxgi_device = null;
	IDXGIAdapter *dxgi_adapter = null;
	IDXGIFactory2 *dxgi_factory = null;

	HRESULT result = ID3D11Device_QueryInterface(state->base_device, &IID_IDXGIDevice, &dxgi_device);
	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}

	result = IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);

	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}

	result = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, &dxgi_factory);
	
	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1 = { 0 };
	swap_chain_desc1.Width = os_window->client_width;
	swap_chain_desc1.Height = os_window->client_height;
	swap_chain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc1.Stereo = FALSE;
	swap_chain_desc1.SampleDesc.Count = 1;
	swap_chain_desc1.SampleDesc.Quality = 0;
	swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc1.BufferCount = 2;
	swap_chain_desc1.Scaling = DXGI_SCALING_STRETCH;
	swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swap_chain_desc1.Flags = 0;

	result = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)state->base_device,
												  os_window->handle, &swap_chain_desc1,
												  null, null, &(state->swap_chain));
	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}
	
	result = IDXGIFactory2_MakeWindowAssociation(dxgi_factory, os_window->handle, DXGI_MWA_NO_ALT_ENTER);
	
	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}

	result = IDXGISwapChain1_GetBuffer(state->swap_chain, 0,
									   &IID_ID3D11Texture2D,
									   &(state->back_buffer));
	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}

	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = { 0 };
	rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtv_desc.Texture2D.MipSlice = 0;
	
	result = ID3D11Device1_CreateRenderTargetView(state->main_device,
										  		  (ID3D11Resource *)state->back_buffer,
			  							 		  &rtv_desc, &(state->back_buffer_as_rtv));
	if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}

	IDXGIDevice_Release(dxgi_device);
	IDXGIAdapter_Release(dxgi_adapter);
	IDXGIFactory2_Release(dxgi_factory);
}

function void
d3d11_initialize(D3D11_State *d3d11_state, OS_Window *os_window) {
	D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	HRESULT result = D3D11CreateDevice(null, D3D_DRIVER_TYPE_HARDWARE, null,
					 				   D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG |
					 				   D3D11_CREATE_DEVICE_BGRA_SUPPORT,
									   &feature_level, 1, D3D11_SDK_VERSION,
									   &(d3d11_state->base_device), null, 
									   &(d3d11_state->base_device_context));

	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	result = ID3D11Device_QueryInterface(d3d11_state->base_device,
										 &IID_ID3D11Device1,
										 &(d3d11_state->main_device));

	ID3D11InfoQueue *d3d11_info_queue = null;
	result = ID3D11Device1_QueryInterface(d3d11_state->main_device, &IID_ID3D11InfoQueue, &d3d11_info_queue);
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	ID3D11InfoQueue_SetBreakOnSeverity(d3d11_info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	ID3D11InfoQueue_SetBreakOnSeverity(d3d11_info_queue, D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
	ID3D11InfoQueue_SetBreakOnSeverity(d3d11_info_queue, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
	ID3D11InfoQueue_Release(d3d11_info_queue);

	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	d3d11_create_swap_chain(d3d11_state, os_window);

	D3D11_RASTERIZER_DESC1 raster_desc1 = { 0 };
	raster_desc1.AntialiasedLineEnable = FALSE;
	raster_desc1.CullMode = D3D11_CULL_BACK;
	raster_desc1.DepthBias = 0;
	raster_desc1.DepthBiasClamp = 0.0f;
	raster_desc1.DepthClipEnable = TRUE;
	raster_desc1.FillMode = D3D11_FILL_SOLID;
	raster_desc1.ForcedSampleCount = 0;
	raster_desc1.FrontCounterClockwise = FALSE;
	raster_desc1.MultisampleEnable = FALSE;
	raster_desc1.ScissorEnable = FALSE;
	raster_desc1.SlopeScaledDepthBias  = 0;
	result = ID3D11Device1_CreateRasterizerState1(d3d11_state->main_device, &raster_desc1, &(d3d11_state->fill_cull_raster));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}
	
	raster_desc1.AntialiasedLineEnable = FALSE;
	raster_desc1.CullMode = D3D11_CULL_NONE;
	raster_desc1.DepthBias = 0;
	raster_desc1.DepthBiasClamp = 0.0f;
	raster_desc1.DepthClipEnable = TRUE;
	raster_desc1.FillMode = D3D11_FILL_WIREFRAME;
	raster_desc1.ForcedSampleCount = 0;
	raster_desc1.FrontCounterClockwise = FALSE;
	raster_desc1.MultisampleEnable = FALSE;
	raster_desc1.ScissorEnable = FALSE;
	raster_desc1.SlopeScaledDepthBias  = 0;
	result = ID3D11Device1_CreateRasterizerState1(d3d11_state->main_device, &raster_desc1,
												  &(d3d11_state->wire_nocull_raster));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	D3D11_TEXTURE2D_DESC depth_buffer_desc;
	ID3D11Texture2D_GetDesc(d3d11_state->back_buffer, &depth_buffer_desc);
	depth_buffer_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	result = ID3D11Device1_CreateTexture2D(d3d11_state->main_device, &depth_buffer_desc,
										   null, &(d3d11_state->depth_buffer_texture));

	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	result = ID3D11Device1_CreateDepthStencilView(d3d11_state->main_device,
												  (ID3D11Resource *)d3d11_state->depth_buffer_texture,
												  null, &(d3d11_state->depth_buffer_view));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	D3D11_DEPTH_STENCIL_DESC depth_buffer_state_desc = { 0 };
	depth_buffer_state_desc.DepthEnable = TRUE; // NODEPTH FORNOW
	depth_buffer_state_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depth_buffer_state_desc.DepthFunc = D3D11_COMPARISON_LESS;
	depth_buffer_state_desc.StencilEnable = FALSE;
	result = ID3D11Device1_CreateDepthStencilState(d3d11_state->main_device,
												   &depth_buffer_state_desc,
												   &(d3d11_state->depth_buffer_state));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        PSTR lpCmdLine, int nCmdShow) {
    unused(hInstance);
    unused(hPrevInstance);
    unused(lpCmdLine);
    unused(nCmdShow);
	
	WNDCLASSA window_class = { 0 };
	window_class.style = CS_HREDRAW | CS_VREDRAW;
  	window_class.lpfnWndProc = &w32_window_proc;
  	window_class.cbClsExtra = 0;
  	window_class.cbWndExtra = 0;
	window_class.hInstance = hInstance;
  	window_class.hIcon = LoadIconA(null, IDI_APPLICATION);
  	window_class.hCursor = LoadCursorA(null, IDC_ARROW);
  	window_class.hbrBackground = null;
  	window_class.lpszMenuName = null;
  	window_class.lpszClassName = "my_window_class";

	if (RegisterClassA(&window_class)) {
		OS_Window os_window = os_create_window(str8("RTR"), 1280, 720);
		OS_Input os_input = { 0 };
		
		D3D11_State d3d11_state;
		d3d11_initialize(&d3d11_state, &os_window);

		ID3D11VertexShader *my_vertex_shader = null;
		ID3D11PixelShader *my_pixel_shader = null;
		ID3D11InputLayout *per_vertex_input_layout = null;
		ID3D11Buffer *cube_vertex_buffer = null;
		ID3D11Buffer *constant_buffer = null;
		ID3D11Buffer *model_instance_buffer = null;
		ID3D11ShaderResourceView *model_instance_srv = null;

		u64 model_instance_count = 0;
		u64 model_instance_capacity = 1024;
		Model_Instance *model_instances = HeapAlloc(GetProcessHeap(),
													HEAP_GENERATE_EXCEPTIONS |
													HEAP_ZERO_MEMORY,
													model_instance_capacity * sizeof(Model_Instance));

		f32 cube_model_vertices[] = {
			// FRONT
			-0.5f, -0.5f, -0.5f,
			-0.5f,  0.5f, -0.5f,
			 0.5f,  0.5f, -0.5f,

			 0.5f,  0.5f, -0.5f,
			 0.5f, -0.5f, -0.5f,
			-0.5f, -0.5f, -0.5f,

			// LEFT
			-0.5f, -0.5f,  0.5f,
			-0.5f,  0.5f,  0.5f,
			-0.5f,  0.5f, -0.5f,

			-0.5f,  0.5f, -0.5f,
			-0.5f, -0.5f, -0.5f,
			-0.5f, -0.5f,  0.5f,

			// BACK
			 0.5f, -0.5f,  0.5f,
			 0.5f,  0.5f,  0.5f,
			-0.5f,  0.5f,  0.5f,

			-0.5f,  0.5f,  0.5f,
			-0.5f, -0.5f,  0.5f,
			 0.5f, -0.5f,  0.5f,

			// RIGHT
			 0.5f, -0.5f, -0.5f,
			 0.5f,  0.5f, -0.5f,
			 0.5f,  0.5f,  0.5f,
			 
			 0.5f,  0.5f,  0.5f,
			 0.5f, -0.5f,  0.5f,
			 0.5f, -0.5f, -0.5f,

			// Top
			-0.5f,  0.5f, -0.5f,
			-0.5f,  0.5f,  0.5f,
			 0.5f,  0.5f,  0.5f,
			 
			 0.5f,  0.5f,  0.5f,
			 0.5f,  0.5f, -0.5f,
			-0.5f,  0.5f, -0.5f,

			// Bottom
			-0.5f, -0.5f,  0.5f,
			-0.5f, -0.5f, -0.5f,
			 0.5f, -0.5f, -0.5f,
			 
			 0.5f, -0.5f, -0.5f,
			 0.5f, -0.5f,  0.5f,
			-0.5f, -0.5f,  0.5f,
		};

		{
			D3D11_BUFFER_DESC cube_mesh_desc = { 0 };
			cube_mesh_desc.ByteWidth = sizeof(cube_model_vertices);
		  	cube_mesh_desc.Usage = D3D11_USAGE_IMMUTABLE;
		  	cube_mesh_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		  	cube_mesh_desc.CPUAccessFlags = 0;
		  	cube_mesh_desc.MiscFlags = 0;
		  	cube_mesh_desc.StructureByteStride = 0;
			
			D3D11_SUBRESOURCE_DATA cube_mesh_data = { 0 };
			cube_mesh_data.pSysMem = cube_model_vertices;
			cube_mesh_data.SysMemPitch = sizeof(f32) * 3;

			HRESULT h_result =  ID3D11Device1_CreateBuffer(d3d11_state.main_device, &cube_mesh_desc,
														   &cube_mesh_data, &cube_vertex_buffer);
			
			if (h_result != S_OK) {
				ExitProcess(0);
			}

			const char hlsl_code[] =
				"#line " stringify(__LINE__) "\n"
				"cbuffer Constants : register(b0) {\n"
				"	float4x4 perspective;"
				"};\n"
				"\n"
				"struct Per_Vertex {\n"
				"	float3 vertex : Vertex;"
				"};\n"
				"\n"
				"struct Model_Per_Instance {\n"
				"	float3 w_p : World_Position;\n"
				"	float4 orient : Quat_Orient;\n"
				"	float3 scale : Scale;"
				"	float4 colour : Colour;\n"
				"};\n"
				"\n"
				"struct VS_Out {\n"
				"	float4 pos : SV_Position;\n"
				"	float4 colour : Colour;\n"
				"};\n"
				"\n"
				"StructuredBuffer<Model_Per_Instance> model_instances : register(t0);\n"
				"\n"
				"float4 quat_mul(float4 a, float4 b) {\n"
				"	float4 result;\n"
				"	result.x = a.x * b.x - dot(a.yzw, b.yzw);\n"
				"	result.yzw = b.yzw * a.x + a.yzw * b.x + cross(a.yzw, b.yzw);\n"
				"	return result;"
				"}\n"
				"\n"
				"float4 quat_conj(float4 a) {\n"
				"	float4 result;\n"
				"	result.x = a.x;\n"
				"	result.yzw = -a.yzw;\n"
				"	return(result);\n"
				"}\n"
				"\n"
				"// assumes unit quaternion!\n"
				"float3 quat_rot_v3f(float4 orient, float3 v) {\n"
				"	return quat_mul(quat_mul(orient, float4(1.0f, v)), quat_conj(orient)).yzw;\n"
				"}\n"
				"\n"
				"VS_Out vs_main(Per_Vertex vertex, uint iid : SV_InstanceID) {\n"
				"	Model_Per_Instance instance = model_instances[iid];\n"
				"	float3 vert = quat_rot_v3f(instance.orient, vertex.vertex) * instance.scale;\n"
				"	VS_Out output = (VS_Out)0;\n"
				"	output.pos = mul(perspective, float4(vert + instance.w_p, 1.0f));\n"
				"	output.colour = instance.colour;\n"
				"	return(output);\n"
				"}\n"
				"\n"
				"float4 ps_main(VS_Out vs) : SV_Target {\n"
				"	return float4(pow(vs.colour.xyz, 2.2f), vs.colour.w);\n"
				"}\n"
				;

			OutputDebugStringA(hlsl_code);

			ID3DBlob *d3d_bytecode = null;
			ID3DBlob *d3d_error = null;
			h_result = D3DCompile(hlsl_code, sizeof(hlsl_code), null, null,
								  D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0",
								  D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
								  D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS |
								  D3DCOMPILE_WARNINGS_ARE_ERRORS, 0,
								  &d3d_bytecode, &d3d_error);

			if (h_result != S_OK) {
				os_message_box(str8("Vertex Shader Compilation Error"),
							   str8_make(ID3D10Blob_GetBufferPointer(d3d_error),
							   			 ID3D10Blob_GetBufferSize(d3d_error)));
				ExitProcess(1);
			}

			h_result = ID3D11Device1_CreateVertexShader(d3d11_state.main_device, ID3D10Blob_GetBufferPointer(d3d_bytecode),
														ID3D10Blob_GetBufferSize(d3d_bytecode), null, &my_vertex_shader);

			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Vertex Shader"));
				ExitProcess(1);
			}

			D3D11_INPUT_ELEMENT_DESC per_vertex_ia[] = {
				{ "Vertex", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};

			h_result = ID3D11Device1_CreateInputLayout(d3d11_state.main_device, per_vertex_ia, 
													   array_count(per_vertex_ia), ID3D10Blob_GetBufferPointer(d3d_bytecode),
													   ID3D10Blob_GetBufferSize(d3d_bytecode), &per_vertex_input_layout);
			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Per-Vertex Input Layout"));
				ExitProcess(1);
			}

			ID3D10Blob_Release(d3d_bytecode);

			h_result = D3DCompile(hlsl_code, sizeof(hlsl_code), null, null,
								  D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0",
								  D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
								  D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS |
								  D3DCOMPILE_WARNINGS_ARE_ERRORS, 0,
								  &d3d_bytecode, &d3d_error);

			if (h_result != S_OK) {
				os_message_box(str8("Pixel Shader Compilation Error"), str8_make(ID3D10Blob_GetBufferPointer(d3d_error), ID3D10Blob_GetBufferSize(d3d_error)));
				ExitProcess(1);
			}

			h_result = ID3D11Device1_CreatePixelShader(d3d11_state.main_device, ID3D10Blob_GetBufferPointer(d3d_bytecode),
													   ID3D10Blob_GetBufferSize(d3d_bytecode), null, &my_pixel_shader);

			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Pixel Shader"));
				ExitProcess(1);
			}

			ID3D10Blob_Release(d3d_bytecode);
		}

		{
			D3D11_BUFFER_DESC model_instance_desc = { 0 };
			model_instance_desc.ByteWidth = (UINT)(model_instance_capacity * sizeof(Model_Instance));
			model_instance_desc.Usage = D3D11_USAGE_DYNAMIC;
  			model_instance_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  			model_instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  			model_instance_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		 	model_instance_desc.StructureByteStride = sizeof(Model_Instance);

			HRESULT h_result = ID3D11Device1_CreateBuffer(d3d11_state.main_device, &model_instance_desc,
														  null, &model_instance_buffer);

			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Model Instance Buffer"));
				ExitProcess(1);
			}
			
			D3D11_SHADER_RESOURCE_VIEW_DESC model_instance_srv_desc = { 0 };
			model_instance_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			model_instance_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			model_instance_srv_desc.Buffer.NumElements = (UINT)model_instance_capacity;
			model_instance_srv_desc.Buffer.ElementWidth = sizeof(Model_Instance);
			h_result = ID3D11Device1_CreateShaderResourceView(d3d11_state.main_device,
															  (ID3D11Resource *)model_instance_buffer,
															  &model_instance_srv_desc,
															  &model_instance_srv);
			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Model Instance SRV"));
				ExitProcess(1);
			}
		}

		{
			D3D11_BUFFER_DESC constant_desc = { 0 };
			constant_desc.ByteWidth = sizeof(D3D11_Constants);
			constant_desc.Usage = D3D11_USAGE_DYNAMIC;
			constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			constant_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			HRESULT h_result = ID3D11Device1_CreateBuffer(d3d11_state.main_device,
														  &constant_desc, null,
									   					  &constant_buffer);

			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Constant Buffer"));
				ExitProcess(1);
			}
		}

		f32 rot_accum = 0.0f;
		while (!(os_input.flags & OSInput_Flag_Quit)) {
			os_fill_events(&os_input, &os_window);
			
			D3D11_VIEWPORT viewport;
			viewport.Width = (f32)os_window.client_width;
			viewport.Height = (f32)os_window.client_height;
			viewport.MinDepth = 0;
			viewport.MaxDepth = 1;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			
			Model_Instance *model = model_instances + model_instance_count++;
//			quat x = quat_make_rotate_around_axis(rot_accum, v3f_make(1.0f, 0.0f, 0.0f));
//			quat y = quat_make_rotate_around_axis(rot_accum * 2.0f, v3f_make(0.0f, 1.0f, 0.0f));
//			quat z = quat_make_rotate_around_axis(-rot_accum * 0.5f, v3f_make(0.0f, 0.0f, 1.0f));

			quat r = quat_make_rotate_around_axis(rot_accum, v3f_make(1.0f, 1.0f, 1.0f));
			
			model->position = v3f_make(-1.0f, 0.0f, 3.5f);
			model->orient = r;
			model->scale = v3f_make(1.0f, 1.0f, 1.0f);
			model->colour = v4f_make(1.0f, 1.0f, 0.0f, 1.0f);

			rot_accum += 0.01f;

			D3D11_MAPPED_SUBRESOURCE mapped_subresource;
			switch (ID3D11DeviceContext_Map(d3d11_state.base_device_context,
											(ID3D11Resource *)constant_buffer, 0, D3D11_MAP_WRITE_DISCARD,
											0, &mapped_subresource)) {
				case S_OK: {
					f32 aspect = viewport.Height / viewport.Width;
					m44 pers = m44_perspective_lh_z01(radians(66.2f), aspect, 1.0f, 100.0f);
					*((m44 *)mapped_subresource.pData) = pers;

					ID3D11DeviceContext_Unmap(d3d11_state.base_device_context, (ID3D11Resource *)constant_buffer, 0);
				} break;
			}
			
			switch (ID3D11DeviceContext_Map(d3d11_state.base_device_context,
											(ID3D11Resource *)model_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD,
											0, &mapped_subresource)) {
				case S_OK: {
					memory_copy(mapped_subresource.pData, model_instances, sizeof(Model_Instance) * model_instance_count);
					ID3D11DeviceContext_Unmap(d3d11_state.base_device_context, (ID3D11Resource *)model_instance_buffer, 0);
				} break;
			}

			f32 colour[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			ID3D11DeviceContext_ClearRenderTargetView(d3d11_state.base_device_context,
													  d3d11_state.back_buffer_as_rtv,
													  colour);
			ID3D11DeviceContext_ClearDepthStencilView(d3d11_state.base_device_context,
													  d3d11_state.depth_buffer_view,
													  D3D11_CLEAR_DEPTH, 1.0f, 0);

			ID3D11DeviceContext_IASetPrimitiveTopology(d3d11_state.base_device_context,
											   		   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			UINT stride = 3 * sizeof(f32);
			UINT offsets = 0;
			ID3D11DeviceContext_IASetVertexBuffers(d3d11_state.base_device_context, 0, 1,
												   &cube_vertex_buffer, &stride, &offsets);

			ID3D11DeviceContext_IASetInputLayout(d3d11_state.base_device_context, per_vertex_input_layout);

			ID3D11DeviceContext_VSSetShader(d3d11_state.base_device_context,
											my_vertex_shader,
											null, 0);

			ID3D11DeviceContext_VSSetShaderResources(d3d11_state.base_device_context, 0,
													 1, &model_instance_srv);

			ID3D11DeviceContext_VSSetConstantBuffers(d3d11_state.base_device_context,
													 0, 1, &constant_buffer);
				
			ID3D11DeviceContext_RSSetViewports(d3d11_state.base_device_context, 1, &viewport);
			ID3D11DeviceContext_RSSetState(d3d11_state.base_device_context,
										   (ID3D11RasterizerState *)d3d11_state.wire_nocull_raster);

			ID3D11DeviceContext_PSSetShader(d3d11_state.base_device_context,
											my_pixel_shader,
											null, 0);

			ID3D11DeviceContext_OMSetDepthStencilState(d3d11_state.base_device_context,
													   d3d11_state.depth_buffer_state, 0);

			ID3D11DeviceContext_OMSetRenderTargets(d3d11_state.base_device_context, 1,
												   &(d3d11_state.back_buffer_as_rtv),
												   d3d11_state.depth_buffer_view);

			ID3D11DeviceContext_OMSetBlendState(d3d11_state.base_device_context,
											    null, null, 0xffffffff);

			ID3D11DeviceContext_DrawInstanced(d3d11_state.base_device_context,
											  36, (UINT)model_instance_count, 0, 0);

			IDXGISwapChain1_Present(d3d11_state.swap_chain, 1, 0);
			model_instance_count = 0;
		}
	}
	
	ExitProcess(0);
}
