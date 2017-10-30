//
// Created by 李晨曦 on 30/10/2017.
//

#ifndef JVM_SLOT_H
#define JVM_SLOT_H
#include <cstdint>
#include <vector>
#include "Object.h"
class Slot;

class LocalVars;

class OperandStack;

class OperandStack
{
  public:
	uint32_t size;
	std::vector<Slot* > slots;
	explicit OperandStack(uint32_t maxStack);
	void PushInt(int32_t val);
	int32_t PopInt();
	void PushFloat(float val);
	float PopFloat();
	void PushLong(int64_t val);
	float PopLong();
	void PushDouble (double val);
	double PopDouble();
	void PushRef(Object* ref);
	Object* PopRef();
};

class LocalVars
{
  public:
	std::vector<Slot* > slots;
	explicit LocalVars(uint32_t maxLocals);
	void SetInt(uint32_t index, int32_t val);
	int32_t GetInt(uint32_t index);
	void SetFloat(uint32_t index, float val);
	float GetFloat(uint32_t index);
	void SetLong(uint32_t index, int64_t val);
	int64_t GetLong(uint32_t index);
	void SetDouble(uint32_t index, double val);
	double GetDouble(uint32_t index);
	void SetRef(uint32_t index, Object* ref);
	Object* GetRef(uint32_t index);
};

class Slot
{
  public:
	int32_t num;
	Object* ref;
	Slot(int32_t num, Object *ref);
};

inline void LocalVars::SetInt(uint32_t index, int32_t val)
{
	this->slots[index]->num = val;
}

inline int32_t LocalVars::GetInt(uint32_t index)
{
	return this->slots[index]->num;
}

inline void LocalVars::SetFloat(uint32_t index, float val)
{
	this->slots[index]->num = *(int *) (&val);
}

inline float LocalVars::GetFloat(uint32_t index)
{
	return *(float *) &(this->slots[index]->num);
}

inline void LocalVars::SetLong(uint32_t index, int64_t val)
{
	this->slots[index]->num = (int32_t) val;
	this->slots[index + 1]->num = (int32_t) (val >> 32);
}

inline int64_t LocalVars::GetLong(uint32_t index)
{
	auto low = (int64_t) (this->slots[index]->num);
	auto high = (int64_t) (this->slots[index + 1]->num);
	return high << 32 | low;
}

inline void LocalVars::SetDouble(uint32_t index, double val)
{
	int64_t temp;
	memcpy(&temp, &val, 8);
	this->SetLong(index, temp);
}

inline double LocalVars::GetDouble(uint32_t index)
{
	double result;
	result = this->GetLong(index);
	return *(int64_t *) &result;
}

inline void LocalVars::SetRef(uint32_t index, Object *ref)
{
	this->slots[index]->ref = ref;
}

inline Object *LocalVars::GetRef(uint32_t index)
{
	return this->slots[index]->ref;
}

inline void OperandStack::PushInt(int32_t val)
{
	this->slots[this->size]->num = val;
	this->size++;
}

inline int32_t OperandStack::PopInt()
{
	this->size--;
	return this->slots[this->size]->num;
}

inline void OperandStack::PushFloat(float val)
{
	this->slots[this->size]->num = static_cast<int32_t >(val);
	this->size++;
}

inline float OperandStack::PopFloat()
{
	this->size--;
	return static_cast<float >(this->slots[this->size]->num);
}

inline void OperandStack::PushLong(int64_t val)
{
	this->slots[this->size]->num = (int32_t) val;
	this->slots[this->size + 1]->num = (int32_t) (val >> 32);
	this->size += 2;
}

inline float OperandStack::PopLong()
{
	this->size -= 2;
	auto low = (int64_t) this->slots[this->size]->num;
	auto high = (int64_t) this->slots[this->size + 1]->num;
	return high << 32 | low;
}

inline void OperandStack::PushDouble(double val)
{
	this->PushLong(static_cast<int64_t >(val));
}

inline double OperandStack::PopDouble()
{
	return static_cast<double >(this->PopLong());
}

inline void OperandStack::PushRef(Object *ref)
{
	this->slots[this->size]->ref = ref;
	this->size++;
}

inline Object *OperandStack::PopRef()
{
	this->size--;
	auto ref = this->slots[size]->ref;
	this->slots[size]->ref = nullptr;
	return ref;
}

#endif //JVM_SLOT_H