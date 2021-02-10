#pragma once

enum
{
  KEY_UNKNOWN = 0,
  KEY_MOUSE_BUTTON_LEFT = 1,
  KEY_MOUSE_BUTTON_MIDDLE = 2,
  KEY_MOUSE_BUTTON_RIGHT = 3,
  KEY_MOUSE_BUTTON4 = 4,
  KEY_MOUSE_BUTTON5 = 5,

  KEY_BACKSPACE = 8,
  KEY_TAB = 9,
  KEY_INSERT = 10,
  KEY_HOME = 11,
  KEY_PAGE_UP = 12,
  KEY_DELETE = 13,
  KEY_END = 14,
  KEY_PAGE_DOWN = 15,
  KEY_ENTER = 16,

  KEY_LEFT_SHIFT = 17,
  KEY_LEFT_CTRL = 18,
  KEY_LEFT_ALT = 19,
  KEY_RIGHT_SHIFT = 20,
  KEY_RIGHT_CTRL = 21,
  KEY_RIGHT_ALT = 22,

  KEY_LEFT = 23,
  KEY_RIGHT = 24,
  KEY_UP = 25,
  KEY_DOWN = 26,

  KEY_ESCAPE = 27,
  KEY_SPACE = 32,

  KEY_APOSTROPHE = 39,
  KEY_COMMA = 44,
  KEY_MINUS = 45,
  KEY_PERIOD = 46,
  KEY_SLASH = 47,
  KEY_0 = 48,
  KEY_1 = 49,
  KEY_2 = 50,
  KEY_3 = 51,
  KEY_4 = 52,
  KEY_5 = 53,
  KEY_6 = 54,
  KEY_7 = 55,
  KEY_8 = 56,
  KEY_9 = 57,
  KEY_SEMI_COLON = 59,
  KEY_EQUAL = 61,

  KEY_A = 65,
  KEY_B = 66,
  KEY_C = 67,
  KEY_D = 68,
  KEY_E = 69,
  KEY_F = 70,
  KEY_G = 71,
  KEY_H = 72,
  KEY_I = 73,
  KEY_J = 74,
  KEY_K = 75,
  KEY_L = 76,
  KEY_M = 77,
  KEY_N = 78,
  KEY_O = 79,
  KEY_P = 80,
  KEY_Q = 81,
  KEY_R = 82,
  KEY_S = 83,
  KEY_T = 84,
  KEY_U = 85,
  KEY_V = 86,
  KEY_W = 87,
  KEY_X = 88,
  KEY_Y = 89,
  KEY_Z = 90,
  KEY_LEFT_BRACKET = 91,
  KEY_BACKSLASH = 92,
  KEY_RIGHT_BRACKET = 93,
  KEY_GRAVE_ACCENT = 96,

  KEY_F1 = 128,
  KEY_F2 = 129,
  KEY_F3 = 130,
  KEY_F4 = 131,
  KEY_F5 = 132,
  KEY_F6 = 133,
  KEY_F7 = 134,
  KEY_F8 = 135,
  KEY_F9 = 136,
  KEY_F10 = 137,
  KEY_F11 = 138,
  KEY_F12 = 139,

  KEY_NUM0 = 140,
  KEY_NUM1 = 141,
  KEY_NUM2 = 142,
  KEY_NUM3 = 143,
  KEY_NUM4 = 144,
  KEY_NUM5 = 145,
  KEY_NUM6 = 146,
  KEY_NUM7 = 147,
  KEY_NUM8 = 148,
  KEY_NUM9 = 149,
  KEY_NUM_DECIMAL = 150,
  KEY_NUM_ENTER = 151,
  KEY_NUM_PLUS = 152,
  KEY_NUM_MINUS = 153,
  KEY_NUM_MULTIPLY = 154,
  KEY_NUM_DIVIDE = 155,
  MAX_KEYS,
};

struct ButtonState
{
    b32 isDown;
    b32 wasDown;
};

inline b32 WasPressed(ButtonState buttonState)
{
    return (buttonState.isDown && !buttonState.wasDown);
}

inline b32 WasReleased(ButtonState buttonState)
{
    return (!buttonState.isDown && buttonState.wasDown);
}

struct GameInput
{
    i32 mouseRelPosX;
    i32 mouseRelPosY;
    i32 mousePosX;
    i32 mousePosY;
    ButtonState buttonStates[MAX_KEYS];
};

internal void InputBeginFrame(GameInput *input)
{
    for (u32 buttonIndex = 0; buttonIndex < ArrayCount(input->buttonStates);
         ++buttonIndex)
    {
        input->buttonStates[buttonIndex].wasDown =
            input->buttonStates[buttonIndex].isDown;
    }

    input->mouseRelPosX = 0;
    input->mouseRelPosY = 0;
}
