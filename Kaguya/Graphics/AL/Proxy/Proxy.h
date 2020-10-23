#pragma once

/*
	A Proxy is a class that contains the data needed to create an object (essentially a description),
	the proxy must not fall out of scope before the creation of a platform-specific object,
	It should define variables with default but configurable state, providing methods to
	configure those states.

	classes that inherits from Proxy must implement Link method, this method essentially
	links proxy data to api specific data and it can also check for invalid arguments,
	Link method should always be called at the beginning of resource constructor associated with the proxy
*/
class Proxy
{
public:
	Proxy() = default;
	virtual ~Proxy() = default;

protected:
	virtual void Link() = 0;
};