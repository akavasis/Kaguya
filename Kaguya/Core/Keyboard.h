#pragma once
#include <queue>
#include <bitset>

class Keyboard
{
	friend class Window;
public:
	struct Event
	{
		enum class Type
		{
			Invalid,
			Press,
			Release
		} type;
		struct Data
		{
			unsigned char Code;
		} data;

		Event();
		Event(Type type, unsigned char code);
	};

	Keyboard();
	~Keyboard() = default;

	void EnableAutoRepeat();
	void DisableAutoRepeat();
	bool AutoRepeatEnabled() const;

	bool IsKeyPressed(unsigned char KeyCode) const;

	Keyboard::Event ReadKey();
	bool KeyBufferIsEmpty() const;

	unsigned char ReadChar();
	bool CharBufferIsEmpty() const;
private:
	static constexpr unsigned int s_BufferSize = 16u;
	bool m_AutoRepeat;
	std::bitset<256u> m_KeyStates;
	std::queue<Keyboard::Event> m_KeyBuffer;
	std::queue<unsigned char> m_CharBuffer;

	void ResetKeyState();
	void OnKeyPress(unsigned char KeyCode);
	void OnKeyRelease(unsigned char KeyCode);
	void OnChar(unsigned char Char);
	template<class T>
	void TrimBuffer(std::queue<T>& QueueBuffer, unsigned int Limit)
	{
		while (QueueBuffer.size() > Limit)
			QueueBuffer.pop();
	}
};