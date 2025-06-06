/* ================================================================
   $File: $
   $Date: $
   $Revision:
   $Creator: Sebastian Bautista $
   $Notice: $
   ================================================================*/
#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_frame_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_viewport_dimensions
{
    int Width;
    int Height;
};

// TODO(Sebas):  This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_frame_buffer GlobalFrameBuffer;

// NOTE(Sebas): XInputGetState 
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
} 
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(Sebas): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter )
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibrary(L"xinput1_4.dll");
    if(!XInputLibrary)
    {
        // TODO(Sebas): Diagnostic
        XInputLibrary = LoadLibrary(L"xinput1_3.dll");
    } 

    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
        // TODO(Sebas): Diagnostic
    }
    else
    {
        // TODO(Sebas): Diagnostic
    }

}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE(Sebas): Load the library
    HMODULE DSoundLibrary = LoadLibrary(L"dsound.dll");

    if(DSoundLibrary)
    {
        // NOTE(Sebas): GetDirectSound object!
        direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX Format = {};
            Format.wFormatTag = WAVE_FORMAT_PCM;
            Format.nChannels = 2;
            Format.nSamplesPerSec = SamplesPerSecond;
            Format.wBitsPerSample = 16;
            Format.nBlockAlign = (Format.nChannels * Format.wBitsPerSample)/8.0f;
            Format.nAvgBytesPerSec = Format.nSamplesPerSec * Format.nBlockAlign;
            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                // NOTE(Sebas): "Create" a primary buffer
                // TODO(Sebas): More flags?
                DSBUFFERDESC BufferDesc = {};
                BufferDesc.dwSize = sizeof(DSBUFFERDESC);
                BufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                BufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &PrimaryBuffer, 0)))
                {
                    
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&Format)))
                    {
                        // NOTE(Sebas): We have finally set the format!
                        OutputDebugString(L"Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO(Sebas): Diagnostic
                    }
                }
                else
                {
                    // TODO(Sebas): Diagnostic
                }
            }
            else
            {
                // TODO(Sebas): Diagnostic
            }
            // NOTE(Sebas): "Create" a secondary buffer
            DSBUFFERDESC BufferDesc = {};
            BufferDesc.dwSize = sizeof(DSBUFFERDESC);
            BufferDesc.dwBufferBytes = BufferSize;
            BufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;
            BufferDesc.lpwfxFormat = &Format;
            LPDIRECTSOUNDBUFFER SecondaryBuffer;
            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &SecondaryBuffer, 0)))
            {
                OutputDebugString(L"Secondary buffer created successfully.\n");
            }
            else
            {
                // TODO(Sebas): Diagnostic
            }
        }
        else
        {
            // TODO(Sebas): Diagnostic
        }
    }
}

internal win32_viewport_dimensions
Win32GetViewportDimensions(HWND Window)
{
    win32_viewport_dimensions Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return Result;
}

internal void
RenderWeirdGradient(win32_frame_buffer* Buffer, int BlueOffset, int GreenOffset)
{
    uint8* Row = (uint8*)Buffer->Memory;
    for(int y = 0; y < Buffer->Height; y++)
    {

        uint32* Pixel = (uint32*)Row;
        for(int x = 0; x < Buffer->Width; x++)
        {
            uint8 Blue = (uint8)(x + BlueOffset);
            uint8 Green = (uint8)(y + GreenOffset);
            uint8 Red = 0;
            uint8 Alpha = 255;
            *Pixel++ = ((Alpha << 24) | (Red << 16) | (Green << 8) | (Blue)); // BLUE Value
        }
        Row += Buffer->Pitch;
    }
}

internal void
Win32ResizeDIBSection(win32_frame_buffer* Buffer, int Width, int Height)
{

    // TODO(Sebas):  Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

    // NOTE(Sebas):
    // Windows knows this is a top-down bitmap
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BufferMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BufferMemorySize, MEM_COMMIT, PAGE_READWRITE);

}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, 
                           int ViewportWidth, 
                           int ViewportHeight, 
                           win32_frame_buffer* Buffer)
{
    // TODO(Sebas): Aspect ratio correction  
    // TODO(Sebas): Play with stretch modes 
    StretchDIBits(DeviceContext,
                  0, 0, ViewportWidth, ViewportHeight, 
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_SIZE:
        {
            OutputDebugString(L"WM_SIZE\n");
        } break;

        case WM_CLOSE:
        {
            // TODO(Sebas):  Handle this with a message to the user?
            GlobalRunning = false;
            OutputDebugString(L"WM_CLOSE\n");
        } break;

        case WM_DESTROY:
        {
            // TODO(Sebas): Handle this as an error - recreate window?  
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugString(L"WM_ACTIVATEAPP:\n");
        } break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        {
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            #define KeyMessageWasDownBit (1 << 30)
            #define KeyMessageIsDownBit (1 << 31)
            bool WasDown = ((LParam & KeyMessageWasDownBit) != 0);
            bool IsDown = ((LParam & KeyMessageIsDownBit) == 0);
            if(WasDown != IsDown)
            {
                if(VKCode == 'W')
                {
                }
                if(VKCode == 'A')
                {
                }
                if(VKCode == 'S')
                {
                }
                if(VKCode == 'D')
                {
                }
                if(VKCode == 'Q')
                {
                }
                if(VKCode == 'E')
                {
                }
                if(VKCode == VK_UP)
                {
                }
                if(VKCode == VK_DOWN)
                {
                }
                if(VKCode == VK_LEFT)
                {
                }
                if(VKCode == VK_RIGHT)
                {
                }
                if(VKCode == VK_ESCAPE)
                {
                    OutputDebugString(L"ESCAPE: ");
                    if(IsDown)
                    {
                        OutputDebugString(L"IsDown");
                    }
                    if(WasDown)
                    {
                        OutputDebugString(L"WasDown");
                    }
                    OutputDebugString(L"\n");
                }
                if(VKCode == VK_SPACE)
                {
                }
            }

            bool32 AltKeyWasDown = (LParam & (1 << 29));
            if ((VKCode == VK_F4) && (AltKeyWasDown))
            {
                GlobalRunning = false;
            }
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_viewport_dimensions Viewport = Win32GetViewportDimensions(Window);
            Win32DisplayBufferInWindow(DeviceContext, Viewport.Width, Viewport.Height, &GlobalFrameBuffer);
            EndPaint(Window, &Paint);
        } break;
        default:
        {
            // OutputDebugString(L"default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return Result;
}

int WINAPI
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    Win32LoadXInput();
    WNDCLASSEXW WindowClass = {};

    Win32ResizeDIBSection(&GlobalFrameBuffer, 1280, 720);

    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = L"HandmadeHeroeWindowClass";

    if (RegisterClassExW(&WindowClass))
    {
        HWND Window =
            CreateWindowExW(
                0,
                WindowClass.lpszClassName,
               L"Handmade Hero",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance,
                0);
        if (Window)
        {
            // NOTE(Sebas): Since we specified CS_OWNDC, we can just get one 
            // device context and use it forever because we are not sharing
            // it with anyone
            HDC DeviceContext = GetDC(Window);
            GlobalRunning = true;
            int XOffset = 0;
            int YOffset = 0;

            Win32InitDSound(Window, 48000, 48000 * sizeof(int16) * 2);

            while(GlobalRunning)
            {
                MSG Message;
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                // TODO(Sebas): Should we poll this more frequently
                for(DWORD ControllerIndex = 0; 
                    ControllerIndex < XUSER_MAX_COUNT; 
                    ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;   
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE(Sebas): This controller is plugged in      
                        // TODO(Sebas): See if ControllerState.dwPacketNumber increments to rapidly
                        XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                        bool DPadUp = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool DPadDown = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool DPadLeft = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool DPadRight = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool StartBtn = Pad->wButtons & XINPUT_GAMEPAD_START;
                        bool BackBtn = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool LThumbBtn = Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
                        bool RThumbBtn = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
                        bool LShoulderBtn = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool RShoulderBtn = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
                        bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
                        bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
                        bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;
                        uint8 LeftTrigger = Pad->bLeftTrigger;
                        uint8 RightTrigger = Pad->bRightTrigger;
                        int16 LStickX = Pad->sThumbLX;
                        int16 LStickY = Pad->sThumbLY;
                        int16 RStickX = Pad->sThumbRX;
                        int16 RStickY = Pad->sThumbRY;
                        int16 LThumbDeadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
                        int16 RThumbDeadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

                        XOffset += (LStickX < -LThumbDeadzone || LStickX > LThumbDeadzone) ? LStickX >> 12: 0;
                        YOffset -= (LStickY < -LThumbDeadzone || LStickY > LThumbDeadzone) ? LStickY >> 12: 0;
                    }
                    else
                    {
                        // NOTE(Sebas): This controller is not available
                    }
                }

                // XINPUT_VIBRATION Vibration;
                // Vibration.wLeftMotorSpeed = 30000;
                // Vibration.wRightMotorSpeed = 30000;
                // XInputSetState(0, &Vibration);

                RenderWeirdGradient(&GlobalFrameBuffer, XOffset, YOffset);
                win32_viewport_dimensions Viewport = Win32GetViewportDimensions(Window);
                Win32DisplayBufferInWindow(DeviceContext, Viewport.Width, Viewport.Height, &GlobalFrameBuffer);

            }
        }
        else
        {
            DWORD ErrorCode = GetLastError();
            // TODO(Sebas):  Logging
        }
    }
    else
    {
        // TODO(Sebas):  Logging 
    }

    return 0;
}
