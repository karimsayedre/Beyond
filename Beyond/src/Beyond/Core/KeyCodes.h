#pragma once

#include <ostream>

namespace Beyond
{
	typedef enum class KeyCode : uint16_t
	{
		// From glfw3.h
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	} Key;

	enum class KeyState
	{
		None = -1,
		Pressed,
		Held,
		Released
	};

	enum class CursorMode
	{
		Normal = 0,
		Hidden = 1,
		Locked = 2
	};

	typedef enum class MouseButton : uint16_t
	{
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Left = Button0,
		Right = Button1,
		Middle = Button2
	} Button;


	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, MouseButton button)
	{
		os << static_cast<int32_t>(button);
		return os;
	}
}

// From glfw3.h
#define BEY_KEY_SPACE           ::Beyond::Key::Space
#define BEY_KEY_APOSTROPHE      ::Beyond::Key::Apostrophe    /* ' */
#define BEY_KEY_COMMA           ::Beyond::Key::Comma         /* , */
#define BEY_KEY_MINUS           ::Beyond::Key::Minus         /* - */
#define BEY_KEY_PERIOD          ::Beyond::Key::Period        /* . */
#define BEY_KEY_SLASH           ::Beyond::Key::Slash         /* / */
#define BEY_KEY_0               ::Beyond::Key::D0
#define BEY_KEY_1               ::Beyond::Key::D1
#define BEY_KEY_2               ::Beyond::Key::D2
#define BEY_KEY_3               ::Beyond::Key::D3
#define BEY_KEY_4               ::Beyond::Key::D4
#define BEY_KEY_5               ::Beyond::Key::D5
#define BEY_KEY_6               ::Beyond::Key::D6
#define BEY_KEY_7               ::Beyond::Key::D7
#define BEY_KEY_8               ::Beyond::Key::D8
#define BEY_KEY_9               ::Beyond::Key::D9
#define BEY_KEY_SEMICOLON       ::Beyond::Key::Semicolon     /* ; */
#define BEY_KEY_EQUAL           ::Beyond::Key::Equal         /* = */
#define BEY_KEY_A               ::Beyond::Key::A
#define BEY_KEY_B               ::Beyond::Key::B
#define BEY_KEY_C               ::Beyond::Key::C
#define BEY_KEY_D               ::Beyond::Key::D
#define BEY_KEY_E               ::Beyond::Key::E
#define BEY_KEY_F               ::Beyond::Key::F
#define BEY_KEY_G               ::Beyond::Key::G
#define BEY_KEY_H               ::Beyond::Key::H
#define BEY_KEY_I               ::Beyond::Key::I
#define BEY_KEY_J               ::Beyond::Key::J
#define BEY_KEY_K               ::Beyond::Key::K
#define BEY_KEY_L               ::Beyond::Key::L
#define BEY_KEY_M               ::Beyond::Key::M
#define BEY_KEY_N               ::Beyond::Key::N
#define BEY_KEY_O               ::Beyond::Key::O
#define BEY_KEY_P               ::Beyond::Key::P
#define BEY_KEY_Q               ::Beyond::Key::Q
#define BEY_KEY_R               ::Beyond::Key::R
#define BEY_KEY_S               ::Beyond::Key::S
#define BEY_KEY_T               ::Beyond::Key::T
#define BEY_KEY_U               ::Beyond::Key::U
#define BEY_KEY_V               ::Beyond::Key::V
#define BEY_KEY_W               ::Beyond::Key::W
#define BEY_KEY_X               ::Beyond::Key::X
#define BEY_KEY_Y               ::Beyond::Key::Y
#define BEY_KEY_Z               ::Beyond::Key::Z
#define BEY_KEY_LEFT_BRACKET    ::Beyond::Key::LeftBracket   /* [ */
#define BEY_KEY_BACKSLASH       ::Beyond::Key::Backslash     /* \ */
#define BEY_KEY_RIGHT_BRACKET   ::Beyond::Key::RightBracket  /* ] */
#define BEY_KEY_GRAVE_ACCENT    ::Beyond::Key::GraveAccent   /* ` */
#define BEY_KEY_WORLD_1         ::Beyond::Key::World1        /* non-US #1 */
#define BEY_KEY_WORLD_2         ::Beyond::Key::World2        /* non-US #2 */

/* Function keys */
#define BEY_KEY_ESCAPE          ::Beyond::Key::Escape
#define BEY_KEY_ENTER           ::Beyond::Key::Enter
#define BEY_KEY_TAB             ::Beyond::Key::Tab
#define BEY_KEY_BACKSPACE       ::Beyond::Key::Backspace
#define BEY_KEY_INSERT          ::Beyond::Key::Insert
#define BEY_KEY_DELETE          ::Beyond::Key::Delete
#define BEY_KEY_RIGHT           ::Beyond::Key::Right
#define BEY_KEY_LEFT            ::Beyond::Key::Left
#define BEY_KEY_DOWN            ::Beyond::Key::Down
#define BEY_KEY_UP              ::Beyond::Key::Up
#define BEY_KEY_PAGE_UP         ::Beyond::Key::PageUp
#define BEY_KEY_PAGE_DOWN       ::Beyond::Key::PageDown
#define BEY_KEY_HOME            ::Beyond::Key::Home
#define BEY_KEY_END             ::Beyond::Key::End
#define BEY_KEY_CAPS_LOCK       ::Beyond::Key::CapsLock
#define BEY_KEY_SCROLL_LOCK     ::Beyond::Key::ScrollLock
#define BEY_KEY_NUM_LOCK        ::Beyond::Key::NumLock
#define BEY_KEY_PRINT_SCREEN    ::Beyond::Key::PrintScreen
#define BEY_KEY_PAUSE           ::Beyond::Key::Pause
#define BEY_KEY_F1              ::Beyond::Key::F1
#define BEY_KEY_F2              ::Beyond::Key::F2
#define BEY_KEY_F3              ::Beyond::Key::F3
#define BEY_KEY_F4              ::Beyond::Key::F4
#define BEY_KEY_F5              ::Beyond::Key::F5
#define BEY_KEY_F6              ::Beyond::Key::F6
#define BEY_KEY_F7              ::Beyond::Key::F7
#define BEY_KEY_F8              ::Beyond::Key::F8
#define BEY_KEY_F9              ::Beyond::Key::F9
#define BEY_KEY_F10             ::Beyond::Key::F10
#define BEY_KEY_F11             ::Beyond::Key::F11
#define BEY_KEY_F12             ::Beyond::Key::F12
#define BEY_KEY_F13             ::Beyond::Key::F13
#define BEY_KEY_F14             ::Beyond::Key::F14
#define BEY_KEY_F15             ::Beyond::Key::F15
#define BEY_KEY_F16             ::Beyond::Key::F16
#define BEY_KEY_F17             ::Beyond::Key::F17
#define BEY_KEY_F18             ::Beyond::Key::F18
#define BEY_KEY_F19             ::Beyond::Key::F19
#define BEY_KEY_F20             ::Beyond::Key::F20
#define BEY_KEY_F21             ::Beyond::Key::F21
#define BEY_KEY_F22             ::Beyond::Key::F22
#define BEY_KEY_F23             ::Beyond::Key::F23
#define BEY_KEY_F24             ::Beyond::Key::F24
#define BEY_KEY_F25             ::Beyond::Key::F25

/* Keypad */
#define BEY_KEY_KP_0            ::Beyond::Key::KP0
#define BEY_KEY_KP_1            ::Beyond::Key::KP1
#define BEY_KEY_KP_2            ::Beyond::Key::KP2
#define BEY_KEY_KP_3            ::Beyond::Key::KP3
#define BEY_KEY_KP_4            ::Beyond::Key::KP4
#define BEY_KEY_KP_5            ::Beyond::Key::KP5
#define BEY_KEY_KP_6            ::Beyond::Key::KP6
#define BEY_KEY_KP_7            ::Beyond::Key::KP7
#define BEY_KEY_KP_8            ::Beyond::Key::KP8
#define BEY_KEY_KP_9            ::Beyond::Key::KP9
#define BEY_KEY_KP_DECIMAL      ::Beyond::Key::KPDecimal
#define BEY_KEY_KP_DIVIDE       ::Beyond::Key::KPDivide
#define BEY_KEY_KP_MULTIPLY     ::Beyond::Key::KPMultiply
#define BEY_KEY_KP_SUBTRACT     ::Beyond::Key::KPSubtract
#define BEY_KEY_KP_ADD          ::Beyond::Key::KPAdd
#define BEY_KEY_KP_ENTER        ::Beyond::Key::KPEnter
#define BEY_KEY_KP_EQUAL        ::Beyond::Key::KPEqual

#define BEY_KEY_LEFT_SHIFT      ::Beyond::Key::LeftShift
#define BEY_KEY_LEFT_CONTROL    ::Beyond::Key::LeftControl
#define BEY_KEY_LEFT_ALT        ::Beyond::Key::LeftAlt
#define BEY_KEY_LEFT_SUPER      ::Beyond::Key::LeftSuper
#define BEY_KEY_RIGHT_SHIFT     ::Beyond::Key::RightShift
#define BEY_KEY_RIGHT_CONTROL   ::Beyond::Key::RightControl
#define BEY_KEY_RIGHT_ALT       ::Beyond::Key::RightAlt
#define BEY_KEY_RIGHT_SUPER     ::Beyond::Key::RightSuper
#define BEY_KEY_MENU            ::Beyond::Key::Menu

// Mouse
#define BEY_MOUSE_BUTTON_LEFT    ::Beyond::Button::Left
#define BEY_MOUSE_BUTTON_RIGHT   ::Beyond::Button::Right
#define BEY_MOUSE_BUTTON_MIDDLE  ::Beyond::Button::Middle
