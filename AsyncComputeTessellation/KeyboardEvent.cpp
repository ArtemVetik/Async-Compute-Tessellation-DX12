#include "KeyboardEvent.h"

KeyboardEvent::KeyboardEvent() : type(EventType::Invalid), key(0u)
{
}

KeyboardEvent::KeyboardEvent(const EventType type, const unsigned char key) : type(type), key(key)
{
	 
}

KeyboardEvent::~KeyboardEvent()
{
}

bool KeyboardEvent::isPressed() const
{
	return type == EventType::Press;
}

bool KeyboardEvent::isReleased() const
{
	return type == EventType::Release;
}

bool KeyboardEvent::isValid() const
{
	return type == EventType::Invalid;
}

unsigned char KeyboardEvent::GetKeyCode() const
{
	return key;
}