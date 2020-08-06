#pragma once

class Proxy
{
public:
	Proxy() = default;
	virtual ~Proxy() = default;

protected:
	virtual void Link() = 0;
};