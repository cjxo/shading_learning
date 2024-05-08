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
    b32 is_focus;
} OS_Window;

typedef u8 OS_Input_Flags;
enum {
	OSInput_Flag_Quit = (1 << 0),
};

typedef u16 OS_Input_Interact_Flags;
enum {
	OSInput_Interact_Pressed = (1 << 1),
	OSInput_Interact_Released = (1 << 2),
	OSInput_Interact_Held = (1 << 3),
};

typedef u16 OS_Input_Key;
enum {
	OSInput_Key_Shift,
	OSInput_Key_Space,
	OSInput_Key_Escape,
	OSInput_Key_W,
	OSInput_Key_A,
	OSInput_Key_S,
	OSInput_Key_D,
	OSInput_Key_Up,
	OSInput_Key_Down,
	OSInput_Key_Left,
	OSInput_Key_Right,
	OSInput_Key_Count,
};

typedef struct {
	OS_Input_Flags flags;
	OS_Input_Interact_Flags key_input[OSInput_Key_Count];

	s32 mouse_displace_x, mouse_displace_y;
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

        case WM_KILLFOCUS: {
            if (os_window) {
                os_window->is_focus = False;
            }
        } break;

        case WM_SETFOCUS: {
            if (os_window) {
                os_window->is_focus = True;
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

function OS_Input_Key
w32_map_wparam_to_input_key(WPARAM wparam) {
	OS_Input_Key result;

	switch (wparam) {
		case VK_SHIFT: {
			result = OSInput_Key_Shift;
		} break;

		case VK_SPACE: {
			result = OSInput_Key_Space;
		} break;

		case VK_ESCAPE: {
			result = OSInput_Key_Escape;
		} break;

		case 'W': {
			result = OSInput_Key_W;
		} break;
		
		case 'A': {
			result = OSInput_Key_A;
		} break;

		case 'S': {
			result = OSInput_Key_S;
		} break;
		
		case 'D': {
			result = OSInput_Key_D;
		} break;

		case VK_UP: {
			result = OSInput_Key_Up;
		} break;
		
		case VK_DOWN: {
			result = OSInput_Key_Down;
		} break;
		
		case VK_LEFT: {
			result = OSInput_Key_Left;
		} break;

		case VK_RIGHT: {
			result = OSInput_Key_Right;
		} break;
		
		default: {
			result = OSInput_Key_Count;
		} break;
	}

	return(result);
}

function void
os_fill_events(OS_Input *input, OS_Window *window) {
	for (u64 key_index = 0; key_index < array_count(input->key_input); ++key_index) {
		input->key_input[key_index] &= ~(OSInput_Interact_Pressed | OSInput_Interact_Released);
	}

	window->resized_this_frame = False;
	SetWindowLongPtrA(window->handle, GWLP_USERDATA, (LONG_PTR)window);
    
	MSG message;
	while (PeekMessageA(&message, null, 0, 0, PM_REMOVE) != 0) {
		switch (message.message) {
			case WM_QUIT: {
				input->flags |= OSInput_Flag_Quit;
			} break;
			
			case WM_KEYDOWN: {
				OS_Input_Key key = w32_map_wparam_to_input_key(message.wParam);
				if (key != OSInput_Key_Count) {
					input->key_input[key] |= (OSInput_Interact_Pressed | OSInput_Interact_Held);
				}
			} break;
			
			case WM_KEYUP: {
				OS_Input_Key key = w32_map_wparam_to_input_key(message.wParam);
				if (key != OSInput_Key_Count) {
					input->key_input[key] |= (OSInput_Interact_Released);
					input->key_input[key] &= ~(OSInput_Interact_Held);
				}
			} break;
            
			default: {
				TranslateMessage(&message);
				DispatchMessage(&message);
			} break;
		}
	}

    if (window->is_focus) {
        POINT cursor_p;
        GetCursorPos(&cursor_p);
        ScreenToClient(window->handle, &cursor_p);

        input->mouse_displace_x = cursor_p.x - (window->client_width / 2);
        input->mouse_displace_y = cursor_p.y - (window->client_height / 2);

        POINT new_cursor;
        new_cursor.x = window->client_width / 2;
        new_cursor.y = window->client_height / 2;
        ClientToScreen(window->handle, &new_cursor);
        SetCursorPos(new_cursor.x, new_cursor.y);

        SetCursor(null);
    }
}

function b32
os_input_pressed(OS_Input *input, OS_Input_Key key) {
	b32 result = (input->key_input[key] & OSInput_Interact_Pressed) != 0;
	return(result);
}

function b32
os_input_released(OS_Input *input, OS_Input_Key key) {
	b32 result = (input->key_input[key] & OSInput_Interact_Released) != 0;
	return(result);
}

function b32
os_input_held(OS_Input *input, OS_Input_Key key) {
	b32 result = (input->key_input[key] & OSInput_Interact_Held) != 0;
	return(result);
}

typedef struct {
	ID3D11Device *base_device;
	ID3D11Device1 *main_device;
	ID3D11DeviceContext *base_device_context;
	IDXGISwapChain1 *swap_chain;
	ID3D11Texture2D *back_buffer;
    ID3D11Texture2D *offscreen_back_buffer;
    ID3D11RenderTargetView *offscreen_back_buffer_rtv;
	ID3D11RenderTargetView *back_buffer_as_rtv;
	ID3D11RasterizerState1 *fill_cull_raster;
	ID3D11RasterizerState1 *wire_nocull_raster;
	ID3D11RasterizerState1 *wire_cull_raster;
	ID3D11Texture2D *depth_buffer_texture;
	ID3D11DepthStencilView *depth_buffer_view;
	ID3D11DepthStencilState *depth_buffer_state;
} D3D11_State;

enum {
    LightType_Directional,
    LightType_Point,
    LightType_Spotlight,
    LightType_Count
};

typedef struct {
    v3f p;
    u32 type;

    f32 reference_distance;
    f32 max_distance;
    f32 min_distance;
    f32 __unused_a;

    v3f direction;
    u32 enabled;

    f32 inner_angle;
    f32 max_angle;
    f32 __unused_c[2];

    v4f colour;
} Light;

__declspec(align(16)) typedef struct {
	m44 perspective;
	m44 world_to_camera;
    v3f camera_p;
    f32 __unused_a;
} D3D11_Constants;

__declspec(align(16)) typedef struct {
    Light light[8];
    v3f camera_p;
    f32 __unused_a;
} Light_Constants;

typedef struct {
    v4f colour;
} Material;

typedef struct {
	v3f position;
	quat orient;
	v3f scale;
	v4f colour;
} Model_Instance;

#define multisample_count 4
#define multisample_quality 0

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
    
    D3D11_TEXTURE2D_DESC backbuffer_desc = { 0 };
    ID3D11Texture2D_GetDesc(state->back_buffer, &backbuffer_desc);
    D3D11_TEXTURE2D_DESC multisampled_offscreen_desc = {0};
    multisampled_offscreen_desc.Width = swap_chain_desc1.Width;
    multisampled_offscreen_desc.Height = swap_chain_desc1.Height;
    multisampled_offscreen_desc.MipLevels = 1;
    multisampled_offscreen_desc.ArraySize = 1;
    multisampled_offscreen_desc.Format = backbuffer_desc.Format;

    // https://learn.microsoft.com/en-us/windows/win32/api/dxgicommon/ns-dxgicommon-dxgi_sample_desc
    multisampled_offscreen_desc.SampleDesc.Count = multisample_count;
    multisampled_offscreen_desc.SampleDesc.Quality = multisample_quality;
    multisampled_offscreen_desc.Usage = D3D11_USAGE_DEFAULT;
    multisampled_offscreen_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    multisampled_offscreen_desc.CPUAccessFlags = 0;
    result = ID3D11Device1_CreateTexture2D(state->main_device, &multisampled_offscreen_desc, null, &(state->offscreen_back_buffer));
    if (result != S_OK) {
		// TODO(christian): Log
		ExitProcess(0);
	}
    
    D3D11_RENDER_TARGET_VIEW_DESC offscreen_back_buffer_view_desc = { 0 };
    offscreen_back_buffer_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    offscreen_back_buffer_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
    result = ID3D11Device1_CreateRenderTargetView(state->main_device, (ID3D11Resource *)state->offscreen_back_buffer,
                                                  &offscreen_back_buffer_view_desc, &(state->offscreen_back_buffer_rtv));
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
	raster_desc1.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
	raster_desc1.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
	raster_desc1.DepthClipEnable = TRUE;
	raster_desc1.FillMode = D3D11_FILL_SOLID;
	raster_desc1.ForcedSampleCount = 0;
	raster_desc1.FrontCounterClockwise = FALSE;
	raster_desc1.MultisampleEnable = TRUE;
	raster_desc1.ScissorEnable = FALSE;
	raster_desc1.SlopeScaledDepthBias  = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	result = ID3D11Device1_CreateRasterizerState1(d3d11_state->main_device, &raster_desc1, &(d3d11_state->fill_cull_raster));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}
	
	raster_desc1.AntialiasedLineEnable = TRUE;
	raster_desc1.CullMode = D3D11_CULL_NONE;
	raster_desc1.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
	raster_desc1.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
	raster_desc1.DepthClipEnable = TRUE;
	raster_desc1.FillMode = D3D11_FILL_WIREFRAME;
	raster_desc1.ForcedSampleCount = 0;
	raster_desc1.FrontCounterClockwise = FALSE;
	raster_desc1.MultisampleEnable = FALSE;
	raster_desc1.ScissorEnable = FALSE;
	raster_desc1.SlopeScaledDepthBias  = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	result = ID3D11Device1_CreateRasterizerState1(d3d11_state->main_device, &raster_desc1,
												  &(d3d11_state->wire_nocull_raster));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}
    
	raster_desc1.CullMode = D3D11_CULL_BACK;
    result = ID3D11Device1_CreateRasterizerState1(d3d11_state->main_device, &raster_desc1,
												  &(d3d11_state->wire_cull_raster));
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}

	D3D11_TEXTURE2D_DESC depth_buffer_desc;
	ID3D11Texture2D_GetDesc(d3d11_state->back_buffer, &depth_buffer_desc);
	depth_buffer_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_buffer_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_buffer_desc.SampleDesc.Count = multisample_count;
    depth_buffer_desc.SampleDesc.Quality = multisample_quality;
    
	result = ID3D11Device1_CreateTexture2D(d3d11_state->main_device, &depth_buffer_desc,
										   null, &(d3d11_state->depth_buffer_texture));
    
	if (result != S_OK) {
		// LOG and CRASH
		ExitProcess(1);
	}
    
    D3D11_DEPTH_STENCIL_VIEW_DESC depth_buffer_view_desc = { 0 };
    depth_buffer_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_buffer_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
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

typedef struct {
    Model_Instance *instances;
    u64 capacity;
    u64 count;
} R3D_Buffer;

function void
r3d_init(R3D_Buffer *buffer, u64 capacity) {
    buffer->count = 0;
    buffer->capacity = capacity;
    buffer->instances = HeapAlloc(GetProcessHeap(),
                                  HEAP_GENERATE_EXCEPTIONS |
                                  HEAP_ZERO_MEMORY,
                                  capacity * sizeof(Model_Instance));
}

function Model_Instance *
r3d_acquire(R3D_Buffer *buffer) {
    s_assert(buffer->count < buffer->capacity, "Must be less than capacity");

    Model_Instance *result = buffer->instances + buffer->count++;
    return(result);
}

function Model_Instance *
r3d_add_instance(R3D_Buffer *buffer, v3f p, quat orient, v3f scale, v4f colour) {
    Model_Instance *model = r3d_acquire(buffer);
    model->position = p;
    model->orient = orient;
    model->scale = scale;
    model->colour = colour;
    return(model);
}

// https://en.wikipedia.org/wiki/Anti-aliasing
// https://en.wikipedia.org/wiki/Multisample_anti-aliasing
// https://en.wikipedia.org/wiki/Supersampling
// https://github.com/microsoft/DirectXTK/wiki/Line-drawing-and-anti-aliasing
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
        os_window.is_focus = True;
		
		D3D11_State d3d11_state;
		d3d11_initialize(&d3d11_state, &os_window);
        
        R3D_Buffer r3d_buffer;
        r3d_init(&r3d_buffer, 1024);
        
		ID3D11VertexShader *my_vertex_shader = null;
		ID3D11PixelShader *my_gooch_pixel_shader = null;
		ID3D11PixelShader *my_test_pixel_shader = null;
		ID3D11InputLayout *per_vertex_input_layout = null;
		ID3D11Buffer *cube_vertex_buffer = null;
		ID3D11Buffer *constant_buffer = null;
		ID3D11Buffer *light_constant_buffer = null;
		ID3D11Buffer *model_instance_buffer = null;
		ID3D11ShaderResourceView *model_instance_srv = null;
         
			// Vertices <-> Normal
		f32 cube_model_vertices[] = {
			// FRONT			
			-0.5f, -0.5f, -0.5f, 	0.0f, 0.0f, -1.0f,
			-0.5f,  0.5f, -0.5f,	0.0f, 0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,	0.0f, 0.0f, -1.0f,
            
             0.5f,  0.5f, -0.5f,	0.0f, 0.0f, -1.0f,
             0.5f, -0.5f, -0.5f,	0.0f, 0.0f, -1.0f,
			-0.5f, -0.5f, -0.5f,	0.0f, 0.0f, -1.0f,
            
			// LEFT
			-0.5f, -0.5f,  0.5f,	-1.0f, 0.0f, 0.0f,
			-0.5f,  0.5f,  0.5f,	-1.0f, 0.0f, 0.0f,
			-0.5f,  0.5f, -0.5f,	-1.0f, 0.0f, 0.0f,
            
			-0.5f,  0.5f, -0.5f,	-1.0f, 0.0f, 0.0f,
			-0.5f, -0.5f, -0.5f,	-1.0f, 0.0f, 0.0f,
			-0.5f, -0.5f,  0.5f,	-1.0f, 0.0f, 0.0f,
            
			// BACK
            0.5f, -0.5f,  0.5f,		0.0f, 0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,	0.0f, 0.0f, 1.0f,
			-0.5f,  0.5f,  0.5f,	0.0f, 0.0f, 1.0f,
            
			-0.5f,  0.5f,  0.5f,	0.0f, 0.0f, 1.0f,
			-0.5f, -0.5f,  0.5f,	0.0f, 0.0f, 1.0f,
             0.5f, -0.5f,  0.5f,	0.0f, 0.0f, 1.0f,
            
			// RIGHT
            0.5f, -0.5f, -0.5f,		1.0f, 0.0f, 0.0f,
            0.5f,  0.5f, -0.5f,		1.0f, 0.0f, 0.0f,
            0.5f,  0.5f,  0.5f,		1.0f, 0.0f, 0.0f,
            
            0.5f,  0.5f,  0.5f,		1.0f, 0.0f, 0.0f,
            0.5f, -0.5f,  0.5f,		1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f,		1.0f, 0.0f, 0.0f,
            
			// Top
			-0.5f,  0.5f, -0.5f,	0.0f, 1.0f, 0.0f,
			-0.5f,  0.5f,  0.5f,	0.0f, 1.0f, 0.0f,
            0.5f,  0.5f,  0.5f,		0.0f, 1.0f, 0.0f,
            
            0.5f,  0.5f,  0.5f,		0.0f, 1.0f, 0.0f,
            0.5f,  0.5f, -0.5f,		0.0f, 1.0f, 0.0f,
			-0.5f,  0.5f, -0.5f,	0.0f, 1.0f, 0.0f,
            
			// Bottom
			-0.5f, -0.5f,  0.5f,	0.0f, -1.0f, 0.0f,
			-0.5f, -0.5f, -0.5f,	0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, -0.5f,		0.0f, -1.0f, 0.0f,
            
            0.5f, -0.5f, -0.5f,		0.0f, -1.0f, 0.0f,
            0.5f, -0.5f,  0.5f,		0.0f, -1.0f, 0.0f,
			-0.5f, -0.5f,  0.5f,	0.0f, -1.0f, 0.0f,
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
			cube_mesh_data.SysMemPitch = sizeof(f32) * 6;
            
			HRESULT h_result =  ID3D11Device1_CreateBuffer(d3d11_state.main_device, &cube_mesh_desc,
														   &cube_mesh_data, &cube_vertex_buffer);
			
			if (h_result != S_OK) {
				ExitProcess(0);
			}

            // Some shading models model light in a binary way. That is, what the object looks like
            // in the presence of light or in the absence of (or unaffected by) light. Thus, we need criteria for
            // distinguishing two cases. That is, distance from light sources, shadowing, surface facing away from light, etc.
            //
            // Mathematically speaking, let g be a function of unlit surface, f be a function of lit surface, n be the surface normal,
            // v be the vector to the eye, l be the vector to the light, and c be the shade result. Then,
            // c = g(n, v) + f(l, n, v).
    
			const char hlsl_code[] =
				"#line " stringify(__LINE__) "\n"
                "#define LightType_Directional 0\n"
                "#define LightType_Point 1\n"
                "#define LightType_Spotlight 2\n"
                "#define Total_Lights 8\n"
                "struct Light {\n"
                "   float3 p : Position;\n" // 12
                "   uint type : Light_Type;\n" // 4
                "   // ------ 16 ------ \n"
                "   float reference_distance : Reference_Distance;\n" // 4
                "   float max_distance : Max_Distance;\n" // 4 
                "   float min_distance : Min_Distance;\n" // 4
                "   float __unused_a;\n" // 4
                "   // ------ 16 ------ \n"
                "   float3 direction : Direction;\n" // 12
                "   uint enabled;\n" // 4
                "   // ------ 16 ------ \n"
                "   float inner_angle : InnerAngle;\n"
                "   float max_angle : MaxAngle;\n"
                "   float2 __unused_c;\n"
                "   // ------ 16 ------ \n"
                "   float4 colour : Colour;\n" // 16
                "};\n"
                "\n"
				"cbuffer Constants : register(b0) {\n"
				"	float4x4 perspective;\n"
				"	float4x4 world_to_camera;\n"
                "   float3 camera_p;\n"
                "   float __unused_a;\n"
				"};\n"
                "\n"
                "cbuffer Light_Constants : register(b1) {\n"
                "   Light lights[Total_Lights];\n"
                "   float3 lcamera_p;\n"
                "   float __unused_b;\n"
                "};\n"
                "\n"
				"struct Per_Vertex {\n"
				"	float3 vertex : Vertex;\n"
				"	// this normal is allowed to not be unit. The vertex shader will normalize this.\n"
				"	float3 normal : Normal;\n"
				"};\n"
				"\n"
				"struct Model_Per_Instance {\n"
				"	float3 w_p : World_Position;\n"
				"	float4 orient : Quat_Orient;\n"
				"	float3 scale : Scale;\n"
				"	float4 colour : Colour;\n"
				"};\n"
				"\n"
				"struct VS_Out {\n"
				"	float4 pos : SV_Position;\n"
                "   float3 pos_world : World_Pos;\n"
				"	float4 colour : Colour;\n"
				"	float3 normal : Normal;\n"
				"};\n"
				"\n"
				"StructuredBuffer<Model_Per_Instance> model_instances : register(t0);\n"
				"\n"
				"float4 quat_mul(float4 a, float4 b) {\n"
				"	float4 result;\n"
				"	result.x = a.x * b.x - dot(a.yzw, b.yzw);\n"
				"	result.yzw = b.yzw * a.x + a.yzw * b.x + cross(a.yzw, b.yzw);\n"
				"	return result;\n"
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
				"	VS_Out output = (VS_Out)0;\n"
				"	Model_Per_Instance instance = model_instances[iid];\n"
				"	float3 vert = quat_rot_v3f(instance.orient, vertex.vertex) * instance.scale;\n"
				"	vert += instance.w_p;\n"
                "   output.pos_world = vert;\n"
				"	vert = mul(world_to_camera, float4(vert, 1.0f)).xyz;\n"
				"	output.pos = mul(perspective, float4(vert, 1.0f));\n"
				"	output.colour = instance.colour;\n"
                "\n"
                "   float3 normal = quat_rot_v3f(instance.orient, vertex.normal) * instance.scale;\n"
                "   output.normal = normalize(normal);\n"
				"	return(output);\n"
				"}\n"
				"\n"
				"float4 ps_gooch_main(VS_Out vs) : SV_Target {\n"
                "   float3 light_p = float3(4.0f, 0.0f, 0.0f);\n"
                "   float3 gooch_cool = float3(0.0f, 0.0f, 0.55f) + 0.25f * vs.colour.xyz;\n"
                "   float3 gooch_warm = float3(0.3f, 0.3f, 0.0f) + 0.25f * vs.colour.xyz;\n"
                "   float3 gooch_highlight = float3(1.0f, 1.0f, 1.0f);\n"
                "\n"
                "   float3 to_eye = normalize(camera_p - vs.pos_world);\n"
                "   float3 to_light = normalize(light_p - vs.pos_world);\n"
                "   float t = (dot(to_light, vs.normal) + 1.0f) * 0.5f;\n"
                "   float3 r = normalize(2.0f * dot(vs.normal, to_light) * vs.normal - to_light);\n"
                "   float s = clamp(100.0f * dot(r, to_eye) - 97.0f, 0.0f, 1.0f);\n"
                "\n"
                "   float3 shaded = s * gooch_highlight + (1.0f - s) * (t * gooch_warm + (1.0f - t) * gooch_cool);\n"
				"	return float4(pow(shaded, 2.2f), vs.colour.w);\n"
				"}\n"
                "\n"
                "float windowing(float r, float rmax) {\n"
                "   float result = pow(max(1.0f - pow(r / rmax, 4.0f), 0.0f), 2.0f);\n"
                "   return(result);\n"
                "}\n"
                "\n"
                "float spotlight(float3 light_dir, float3 spot_dir, float inner_circle_angle, float max_angle) {\n"
                "   float c = dot(spot_dir, light_dir);\n"
                "   float result = clamp((c - cos(max_angle)) / (cos(inner_circle_angle) - cos(max_angle)), 0.0f, 1.0f);\n"
                "   //return(result * result);\n"
                "   return result * result * (3.0f - 2.0f * result);\n"
                "}\n"
                "\n"
                "float4 ps_test_shading_model(VS_Out vs) : SV_Target {\n"
                "   float3 unlit_colour = 0.05f * vs.colour.xyz;\n"
                "   float3 lit_colour = vs.colour.xyz;\n"
                "\n"
                "   float3 shaded = (float3)0;\n"
                "   for (uint light_idx = 0; light_idx < Total_Lights; ++light_idx) {\n"
                "       Light light = lights[light_idx];\n"
                "       if (!light.enabled) continue;\n"
                "       float3 light_colour = light.colour.xyz;\n"
                "\n"
                "       if (light.type == LightType_Directional) {\n"
                "           float cosine = max(dot(-light.direction, vs.normal), 0.0f);\n"
                "           shaded += cosine * light_colour * lit_colour;\n"
                "       } else {\n"
                "           float3 to_light = light.p - vs.pos_world;\n"
                "           float distance_to_light = length(to_light);\n"
                "           to_light /= distance_to_light;\n"
                "\n"
                "           // unreal engine's attenuation\n"
                "           //float attenuation = ((r0 * r0) / (1.0f + distance_to_light * distance_to_light));\n"
                "           //CryEngine's attenuation\n"
                "           float wnd = windowing(distance_to_light, light.max_distance);\n"
                "           float attenuation = pow(light.reference_distance / max(distance_to_light, light.min_distance), 2.0f) * wnd;\n"
                "           //float attenuation = windowing(distance_to_light, 50.0f);\n"
                "           float cosine = max(dot(to_light, vs.normal), 0.0f);\n"
                "           if (light.type == LightType_Point) {\n"
                "               shaded += cosine * light_colour * lit_colour * attenuation;\n"
                "           } else {\n"
                "               shaded += cosine * light_colour * lit_colour * attenuation * spotlight(-to_light, normalize(light.direction), radians(light.inner_angle), radians(light.max_angle));\n"
                "           }\n"
                "       }\n"
                "       shaded = saturate(shaded);\n"
                "   }\n"
                "   shaded += unlit_colour;\n"
                "\n"
                "\n" 
				"	return float4(pow(shaded, 2.2f), vs.colour.w);\n"
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
				{ "Vertex", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "Normal", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
								  D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_gooch_main", "ps_5_0",
								  D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
								  D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS |
								  D3DCOMPILE_WARNINGS_ARE_ERRORS, 0,
								  &d3d_bytecode, &d3d_error);
            
			if (h_result != S_OK) {
				os_message_box(str8("Pixel Shader Compilation Error"), str8_make(ID3D10Blob_GetBufferPointer(d3d_error), ID3D10Blob_GetBufferSize(d3d_error)));
				ExitProcess(1);
			}
            
            // The idea of Gooch Shading is to compare the surface normal to the light's location. If the normal points towards the light,
            // a warmer tone is used for the surface. Else if it points away, a cooler tone is used. Angles in between interpolate between these tones.
			h_result = ID3D11Device1_CreatePixelShader(d3d11_state.main_device, ID3D10Blob_GetBufferPointer(d3d_bytecode),
													   ID3D10Blob_GetBufferSize(d3d_bytecode), null, &my_gooch_pixel_shader);
            
			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Pixel Shader"));
				ExitProcess(1);
			}
            
			ID3D10Blob_Release(d3d_bytecode);

            h_result = D3DCompile(hlsl_code, sizeof(hlsl_code), null, null,
								  D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_test_shading_model", "ps_5_0",
								  D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
								  D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS |
								  D3DCOMPILE_WARNINGS_ARE_ERRORS, 0,
								  &d3d_bytecode, &d3d_error);
            
			if (h_result != S_OK) {
				os_message_box(str8("Pixel Shader Compilation Error"), str8_make(ID3D10Blob_GetBufferPointer(d3d_error), ID3D10Blob_GetBufferSize(d3d_error)));
				ExitProcess(1);
			}

            h_result = ID3D11Device1_CreatePixelShader(d3d11_state.main_device, ID3D10Blob_GetBufferPointer(d3d_bytecode),
													   ID3D10Blob_GetBufferSize(d3d_bytecode), null, &my_test_pixel_shader);
            
			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Pixel Shader"));
				ExitProcess(1);
			}
            
			ID3D10Blob_Release(d3d_bytecode);
		}
        
		{
			D3D11_BUFFER_DESC model_instance_desc = { 0 };
			model_instance_desc.ByteWidth = (UINT)(r3d_buffer.capacity * sizeof(Model_Instance));
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
			model_instance_srv_desc.Buffer.NumElements = (UINT)r3d_buffer.capacity;
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

            //
            constant_desc.ByteWidth = sizeof(Light_Constants);
			constant_desc.Usage = D3D11_USAGE_DYNAMIC;
			constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			constant_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			h_result = ID3D11Device1_CreateBuffer(d3d11_state.main_device,
                                                  &constant_desc, null,
                                                  &light_constant_buffer);
    
			if (h_result != S_OK) {
				os_message_box(str8("Error"), str8("Failed to create Constant Buffer"));
				ExitProcess(1);
			}
		}
       
		// Ok, so rotations in R^3 (esp. camera)
		// We discuss Euler Angles, namely, pitch, yaw, and roll.
		// Euler angles are a mechanism for creating a rotation through a sequence
		// of three simpler rotations called pitch, yaw, and roll. 
		// Pitching rotates an object around the x-axis.
		// 	- pointing up or down (yes guesture)
		// Yawing rotates an object around the xz-plane or around the y-axis.
		// 	- turning left or right (no guesture)
		// Rolling rotates an object around the xy-plane or around the z-axis.
		//	- tilting your head left and right.
		//
		//	Rotation matrices are orthonormal. Thus its inverse is its transpose.
		//
		//	We now derive the camera basis. We use pitch and yaw and radius r (sphere coords).
		//	1. Suppose you're at the origin. Look straight ahead towards the x-axis.
		//	2. The y-axis points from your feet to your head. Point your right-arm up the y-axis.
		//	3. Rotate your body counterclockwise by the angle yaw (azimuth).
		//	4. Rotate your right-arm downward by angle pitch (zenith)
		//	5. Displace by the distance r.
		//	Now we convert from sphere coords to cartesian coords.
		v3f camera_p = v3f_make(0.0f, 0.0f, 0.0f);
		f32 camera_theta = 90.0f; // nod yes; Rotate around x.
		f32 camera_phi = 90.0f; // no; rotate around y
	
		{
			POINT new_cursor;
			new_cursor.x = os_window.client_width / 2;
			new_cursor.y = os_window.client_height / 2;
			ClientToScreen(os_window.handle, &new_cursor);
			SetCursorPos(new_cursor.x, new_cursor.y);
		}

        f32 game_dt_step = 1.0f / 60.0f;
		f32 rot_accum = 0.0f;
		while (!(os_input.flags & OSInput_Flag_Quit)) {
			os_fill_events(&os_input, &os_window);

			if (os_input_released(&os_input, OSInput_Key_Escape)) {
				os_input.flags |= OSInput_Flag_Quit;
			}

			f32 mouse_sensitivity = 0.1f;
			camera_theta += os_input.mouse_displace_y * mouse_sensitivity;
			camera_phi -= os_input.mouse_displace_x * mouse_sensitivity;
			
			if (camera_theta > 145.0f) {
				camera_theta = 145.0f;
			} else if (camera_theta < 45.0f) {
				camera_theta = 45.0f;
			}

			if (camera_phi >= 360.0f) {
				camera_phi = 0.0f;
			} else if (camera_phi <= -360.0f) {
				camera_phi = 0.0f;
			}

			f32 theta = radians(camera_theta);
			f32 phi = radians(camera_phi);
			f32 cosine_theta = cosf(theta);
			f32 cosine_phi = cosf(phi);
			f32 sine_theta = sinf(theta);
			f32 sine_phi = sinf(phi);

			v3f camera_forward;
			camera_forward.x = cosine_phi * sine_theta;
			camera_forward.y = cosine_theta;
			camera_forward.z = sine_theta * sine_phi;
			v3f_norm(&camera_forward);

			v3f temp_up = v3f_make(0.0f, 1.0f, 0.0f);
			v3f camera_right = v3f_cross(temp_up, camera_forward);
			v3f_norm(&camera_right);
			v3f camera_up = v3f_cross(camera_forward, camera_right);
			v3f_norm(&camera_up);

			f32 move_speed = 0.1f;
			if (os_input_held(&os_input, OSInput_Key_W)) {
				camera_p = v3f_add(v3f_scale(camera_forward, move_speed), camera_p);
			}
			
			if (os_input_held(&os_input, OSInput_Key_S)) {
				camera_p = v3f_sub(camera_p, v3f_scale(camera_forward, move_speed));
			}
			
			if (os_input_held(&os_input, OSInput_Key_A)) {
				camera_p = v3f_sub(camera_p, v3f_scale(camera_right, move_speed));
			}
			
			if (os_input_held(&os_input, OSInput_Key_D)) {
				camera_p = v3f_add(camera_p, v3f_scale(camera_right, move_speed));
			}

			if (os_input_held(&os_input, OSInput_Key_Shift)) {
				camera_p = v3f_sub(camera_p, v3f_scale(camera_up, move_speed));
			}
			
			if (os_input_held(&os_input, OSInput_Key_Space)) {
				camera_p = v3f_add(camera_p, v3f_scale(camera_up, move_speed));
			}

			D3D11_VIEWPORT viewport;
			viewport.Width = (f32)os_window.client_width;
			viewport.Height = (f32)os_window.client_height;
			viewport.MinDepth = 0;
			viewport.MaxDepth = 1;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			
            r3d_add_instance(&r3d_buffer, v3f_make(0.0f, 0.0f, 8.0f),
                             quat_make_rotate_around_axis(rot_accum, v3f_make(1.0f, 0.0f, 0.0f)),
                             v3f_make(6.0f, 6.0f, 6.0f),
                             v4f_make(0.0f, 0.5f, 0.8f, 1.0f));
            
            r3d_add_instance(&r3d_buffer, v3f_make(6.0f, 0.0f, 4.0f),
                             quat_make_rotate_around_axis(rot_accum * -1.0f, v3f_make(0.0f, 0.5f, 1.0f)),
                             v3f_make(1.0f, 1.0f, 1.0f),
                             v4f_make(0.6f, 0.5f, 0.0f, 1.0f));
            //quat x = quat_make_rotate_around_axis(rot_accum, v3f_make(1.0f, 0.0f, 0.0f));
            //quat y = quat_make_rotate_around_axis(rot_accum * 2.0f, v3f_make(0.0f, 1.0f, 0.0f));
            //quat z = quat_make_rotate_around_axis(-rot_accum * 0.5f, v3f_make(0.0f, 0.0f, 1.0f));

			rot_accum += game_dt_step;
            
			D3D11_MAPPED_SUBRESOURCE mapped_subresource;
			switch (ID3D11DeviceContext_Map(d3d11_state.base_device_context,
                                                    (ID3D11Resource *)constant_buffer, 0, D3D11_MAP_WRITE_DISCARD,
                                                    0, &mapped_subresource)) {
				case S_OK: {
					f32 aspect = viewport.Height / viewport.Width;
                    m44 pers = m44_perspective_lh_z01(radians(66.2f), aspect, 1.0f, 100.0f);
                    D3D11_Constants *constants = ((D3D11_Constants *)mapped_subresource.pData);

					constants->perspective = pers;
					constants->world_to_camera.rows[0] = v4f_make(camera_right.x, camera_up.x, camera_forward.x, 0.0f);
					constants->world_to_camera.rows[1] = v4f_make(camera_right.y, camera_up.y, camera_forward.y, 0.0f);
					constants->world_to_camera.rows[2] = v4f_make(camera_right.z, camera_up.z, camera_forward.z, 0.0f);
					constants->world_to_camera.rows[3].x = -v3f_dot(camera_right, camera_p);
					constants->world_to_camera.rows[3].y = -v3f_dot(camera_up, camera_p);
					constants->world_to_camera.rows[3].z = -v3f_dot(camera_forward, camera_p);
					constants->world_to_camera.rows[3].w = 1;
                    constants->camera_p = camera_p;
					ID3D11DeviceContext_Unmap(d3d11_state.base_device_context, (ID3D11Resource *)constant_buffer, 0);
				} break;
			}

            switch (ID3D11DeviceContext_Map(d3d11_state.base_device_context,
                                            (ID3D11Resource *)light_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD,
                                            0, &mapped_subresource)) {
                case S_OK: {
                    v3f size = v3f_make(0.2f, 0.2f, 0.2f);
                    v4f colour = v4f_make(1.0f, 1.0f, 1.0f, 1.0f);

                    Light_Constants *constants = (Light_Constants *)mapped_subresource.pData;
                    Light light;
                    light.type = LightType_Spotlight;
                    light.p = v3f_make(0.0f, 0.0f, -1.0f);
                    light.reference_distance = 8.0f;
                    light.max_distance = 50.0f;
                    light.min_distance = 1.0f;
                    light.direction = v3f_make(0.0f, 0.0f, 1.0f);
                    light.colour = v4f_make(0.5f, 0.3f, 1.0f, 1.0f);
                    light.inner_angle = 15.0f;
                    light.max_angle = 35.0f;
                    light.enabled = True;
                    constants->light[0] = light;
                    r3d_add_instance(&r3d_buffer, light.p, quat_identity(), size, colour);

                    light.p = v3f_make(16.0f, 4.0f, -4.0f);
                    light.reference_distance = 24.0f;
                    light.max_distance = 100.0f;
                    light.direction = v3f_make(-1.0f, 0.0f, 1.0f);
                    light.colour = v4f_make(1.0f, 1.0f, 1.0f, 1.0f);
                    constants->light[1] = light;
                    r3d_add_instance(&r3d_buffer, light.p, quat_identity(), size, colour);

                    light.type = LightType_Point;
                    light.p = v3f_make(sinf(rot_accum) * 10.0f, cosf(rot_accum) * 10.0f, 0.0f);
                    light.reference_distance = 16.0f;
                    light.max_distance = 100.0f;
                    light.colour = v4f_make(0.0f, 1.0f, 0.0f, 1.0f);
                    constants->light[2] = light;

                    r3d_add_instance(&r3d_buffer, light.p, quat_identity(), size, colour);
                    constants->camera_p = camera_p;
					ID3D11DeviceContext_Unmap(d3d11_state.base_device_context, (ID3D11Resource *)light_constant_buffer, 0);
                } break;
            }
			
			switch (ID3D11DeviceContext_Map(d3d11_state.base_device_context,
											(ID3D11Resource *)model_instance_buffer, 0, D3D11_MAP_WRITE_DISCARD,
											0, &mapped_subresource)) {
				case S_OK: {
					memory_copy(mapped_subresource.pData, r3d_buffer.instances, sizeof(Model_Instance) * r3d_buffer.count);
					ID3D11DeviceContext_Unmap(d3d11_state.base_device_context, (ID3D11Resource *)model_instance_buffer, 0);
				} break;
			}
            
			f32 colour[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			ID3D11DeviceContext_ClearRenderTargetView(d3d11_state.base_device_context,
													  d3d11_state.offscreen_back_buffer_rtv,
													  colour);
			ID3D11DeviceContext_ClearDepthStencilView(d3d11_state.base_device_context,
													  d3d11_state.depth_buffer_view,
													  D3D11_CLEAR_DEPTH, 1.0f, 0);
            
			ID3D11DeviceContext_IASetPrimitiveTopology(d3d11_state.base_device_context,
                                                       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            
			UINT stride = 6 * sizeof(f32);
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
										   (ID3D11RasterizerState *)d3d11_state.fill_cull_raster);
            
            ID3D11DeviceContext_PSSetConstantBuffers(d3d11_state.base_device_context,
                                                     1, 1, &light_constant_buffer);

			ID3D11DeviceContext_PSSetShader(d3d11_state.base_device_context,
											my_test_pixel_shader,
											null, 0);
            
			ID3D11DeviceContext_OMSetDepthStencilState(d3d11_state.base_device_context,
													   d3d11_state.depth_buffer_state, 0);
            
			ID3D11DeviceContext_OMSetRenderTargets(d3d11_state.base_device_context, 1,
												   &(d3d11_state.offscreen_back_buffer_rtv),
												   d3d11_state.depth_buffer_view);
            
			ID3D11DeviceContext_OMSetBlendState(d3d11_state.base_device_context,
											    null, null, 0xffffffff);

			ID3D11DeviceContext_DrawInstanced(d3d11_state.base_device_context,
											  36, (UINT)r3d_buffer.count, 0, 0);


            D3D11_TEXTURE2D_DESC backbuffer_desc = { 0 };
            ID3D11Texture2D_GetDesc(d3d11_state.back_buffer, &backbuffer_desc);
            ID3D11DeviceContext_ResolveSubresource(d3d11_state.base_device_context, (ID3D11Resource *)d3d11_state.back_buffer, 0,
                                                   (ID3D11Resource *)d3d11_state.offscreen_back_buffer, 0, 
                                                   backbuffer_desc.Format);
            
			IDXGISwapChain1_Present(d3d11_state.swap_chain, 1, 0);
			r3d_buffer.count = 0;
		}
	}

	ExitProcess(0);
}
