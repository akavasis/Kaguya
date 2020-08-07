#include "pch.h"
#include "InputLayout.h"

void InputLayout::AddVertexLayoutElement(LPCSTR SemanticName, UINT SemanticIndex, DXGI_FORMAT Format, UINT InputSlot, UINT AlignedByteOffset)
{
	m_InputElementSemanticNames.push_back(SemanticName);

	D3D12_INPUT_ELEMENT_DESC inputElement = {};
	inputElement.SemanticName = nullptr; // Will be resolved later
	inputElement.SemanticIndex = SemanticIndex;
	inputElement.Format = Format;
	inputElement.InputSlot = InputSlot;
	inputElement.AlignedByteOffset = AlignedByteOffset;
	inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElement.InstanceDataStepRate = 0;

	m_InputElements.push_back(inputElement);
}

void InputLayout::AddInstanceLayoutElement(LPCSTR SemanticName, UINT SemanticIndex, DXGI_FORMAT Format, UINT InputSlot, UINT AlignedByteOffset, UINT InstanceDataStepRate)
{
	m_InputElementSemanticNames.push_back(SemanticName);

	D3D12_INPUT_ELEMENT_DESC inputElement = {};
	inputElement.SemanticName = nullptr; // Will be resolved later
	inputElement.SemanticIndex = SemanticIndex;
	inputElement.Format = Format;
	inputElement.InputSlot = InputSlot;
	inputElement.AlignedByteOffset = AlignedByteOffset;
	inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
	inputElement.InstanceDataStepRate = InstanceDataStepRate;

	m_InputElements.push_back(inputElement);
}

InputLayout::operator D3D12_INPUT_LAYOUT_DESC() const noexcept
{
	for (size_t i = 0; i < m_InputElements.size(); ++i)
	{
		m_InputElements[i].SemanticName = m_InputElementSemanticNames[i].data();
	}

	D3D12_INPUT_LAYOUT_DESC desc = {};
	desc.pInputElementDescs = m_InputElements.data();
	desc.NumElements = static_cast<UINT>(m_InputElements.size());

	return desc;
}