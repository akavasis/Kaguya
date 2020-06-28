#include "pch.h"
#include "Exception.h"
#include <sstream>
#include <comdef.h> // _com_error

Exception::Exception(std::string File, int Line) noexcept
	: m_File(std::move(File)), m_Line(Line)
{
}

const char* Exception::what() const noexcept
{
	std::ostringstream oss;
	oss << Type() << std::endl
		<< Origin() << std::endl
		<< Error() << std::endl;
	m_ErrorMessage = oss.str();
	return m_ErrorMessage.data();
}

std::string Exception::Type() const noexcept
{
	return "[Engine Exception]";
}

std::string Exception::Error() const noexcept
{
	return "Unspecified Error";
}

std::string Exception::Origin() const noexcept
{
	std::ostringstream oss;
	oss << "[File] " << m_File << std::endl
		<< "[Line] " << m_Line << std::endl;
	return oss.str();
}

COMException::COMException(std::string File, int Line, HRESULT HR) noexcept
	: Exception(std::move(File), Line), m_HR(HR)
{
}

std::string COMException::Type() const noexcept
{
	return "[COM Exception]";
}

std::string COMException::Error() const noexcept
{
	_com_error comerror(m_HR);
	std::wstring errorMessage = comerror.ErrorMessage();
	return std::string(errorMessage.begin(), errorMessage.end());
}
