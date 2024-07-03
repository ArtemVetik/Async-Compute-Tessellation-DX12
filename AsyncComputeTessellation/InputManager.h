#pragma once
#include <queue>
#include "Windows.h"
#include "KeyboardEvent.h"
#include "Xinput.h"

#pragma comment(lib, "XInput.lib")

class MyMouseEventHandler
{
public:
	virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;
};

class InputManager
{
public:
	InputManager();
	~InputManager();

	static InputManager* getInstance();

	// keyboard
	bool isKeyPressed(const unsigned char keyCode);
	bool KeyBufferEmpty();
	KeyboardEvent ReadKey();
	void OnKeyPressed(const unsigned char key);
	void OnKeyReleased(const unsigned char key);
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void EnableAutoRepeatKeys();
	void DisableAutoRepeatKeys();
	bool isKeysAutoRepeat();

	// controller
	void UpdateController();
	bool isControllerButtonPressed(WORD keyCode);
	SHORT getLeftStickX();
	SHORT getLeftStickY();
	SHORT getRightStickX();
	SHORT getRightStickY();

	void addMouseHandler(MyMouseEventHandler* handler);
	void removeMouseHandler(MyMouseEventHandler* handler);

private:
	bool autoRepeatKeys = false;
	bool keyStates[256];
	DWORD previousControllerState;
	_XINPUT_GAMEPAD gameController;
	std::queue<KeyboardEvent> keyBuffer;
};

